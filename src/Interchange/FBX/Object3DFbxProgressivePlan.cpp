#include "Object3DFbxProgressivePlan.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>

namespace iiActorObject::object3d_detail
{
ProgressiveFbxPlans buildProgressiveFbxPlans(const Object3DAsset &asset,Object3DRigSession &session,
    const ProgressiveMorphProgram &program)
{
    const auto &meshes=asset.data().meshes;
    ProgressiveFbxPlans result;auto &plan=result.shared,&split=result.separate;
    plan.channels.resize(meshes.size());plan.consumedTargets.resize(meshes.size());
    plan.representative.resize(meshes.size());split.channels.resize(meshes.size());split.representative.resize(meshes.size());
    for(std::size_t i=0;i<meshes.size();++i)plan.representative[i]=split.representative[i]=i;
    std::map<std::string,std::vector<std::size_t>,std::less<>> membership;
    for(const auto &channel:program.channels)for(const auto &key:channel.keys)membership.try_emplace(key.target);
    std::vector<std::map<std::string_view,std::size_t,std::less<>>> records(meshes.size());
    std::vector<std::map<std::string_view,double,std::less<>>> assetDefaults(meshes.size());
    constexpr std::size_t maximumVisits=128'000'000;std::size_t visits=0;
    for(std::size_t meshIndex=0;meshIndex<meshes.size();++meshIndex)
    {
        const auto &mesh=meshes[meshIndex];
        if(mesh.geometry.morphTargets.size()>maximumVisits-visits)throw std::length_error("Progressive FBX binding visit budget");
        visits+=mesh.geometry.morphTargets.size();std::set<std::string_view,std::less<>> seen;
        for(const auto &morph:mesh.geometry.morphTargets)
            if(auto found=membership.find(morph.name);found!=membership.end()&&seen.insert(morph.name).second)
                found->second.push_back(meshIndex);
        if(mesh.morphDefaults.size()>maximumVisits-visits)throw std::length_error("Progressive FBX defaults visit budget");
        visits+=mesh.morphDefaults.size();
        for(const auto &entry:mesh.morphDefaults)if(membership.contains(entry.name))assetDefaults[meshIndex].emplace(entry.name,entry.value);
        if(mesh.topology)
        {
            if(mesh.topology->morphTargets.size()>maximumVisits-visits)throw std::length_error("Progressive FBX source record visit budget");
            visits+=mesh.topology->morphTargets.size();
            for(const auto &morph:mesh.topology->morphTargets)
                if(membership.contains(morph.name))++records[meshIndex][morph.name];
        }
    }
    std::map<std::vector<std::size_t>,std::size_t> groups;
    std::vector<std::optional<std::size_t>> assigned(meshes.size());std::size_t comparisons=0;
    const auto account=[&](std::size_t count)
    {if(count>maximumVisits-comparisons)throw std::length_error("Progressive FBX mesh binding comparison budget");comparisons+=count;};
    constexpr std::size_t maximumMeshReferences=65535,maximumKeyInstances=16'000'000;
    std::size_t meshReferences=0,keyInstances=0;
    if(program.channels.size()>65535)throw std::length_error("Progressive FBX channel limit");
    bool &requiresSplit=result.requiresSplit;std::int32_t groupId=0;
    for(const auto &channel:program.channels)
    {
        if(channel.keys.empty())throw std::invalid_argument("Progressive FBX channel has no keys");
        ++groupId;
        const std::vector<std::size_t> *firstOwners=nullptr;std::set<std::size_t> ownerSet;bool sparse=false;
        const auto defaultPercent=100.*channel.defaultValue;
        if(!std::isfinite(defaultPercent))throw std::overflow_error("Progressive FBX default percentage overflow");
        double previous=0;bool first=true;
        for(const auto &key:channel.keys)
        {
            const auto &attached=membership.at(key.target);
            account(attached.size());
            if(attached.empty())throw std::invalid_argument("Progressive FBX target has no mesh");
            if(firstOwners&&*firstOwners!=attached)sparse=true;
            if(!firstOwners)firstOwners=&attached;
            ownerSet.insert(attached.begin(),attached.end());const double percentage=100.*key.fullWeight;
            if(!std::isfinite(percentage)||percentage==0||(!first&&percentage<=previous))
                throw std::overflow_error("Progressive FBX full-weight percentage cannot be represented");
            previous=percentage;first=false;
            for(const auto index:attached)plan.consumedTargets[index].insert(key.target);
        }
        const std::vector<std::size_t> owners(ownerSet.begin(),ownerSet.end());
        if(owners.size()>maximumMeshReferences-meshReferences)
            throw std::length_error("Progressive FBX mesh reference limit");
        meshReferences+=owners.size();
        if(channel.keys.size()>(maximumKeyInstances-keyInstances)/owners.size())
            throw std::length_error("Progressive FBX key instance limit");
        keyInstances+=channel.keys.size()*owners.size();
        if(sparse){requiresSplit=true;split.sparse.insert(&channel);}
        const auto [group,created]=groups.try_emplace(owners,groups.size());(void)created;
        for(const auto index:owners)
        {
            if(assigned[index]&&*assigned[index]!=group->second)
                requiresSplit=true;
            assigned[index]=group->second;plan.representative[index]=owners.front();
            split.channels[index].push_back(&channel);
        }
        plan.channels.at(owners.front()).push_back(&channel);
        if(owners.size()>1)
        {
            if(owners.size()>65535)throw std::length_error("Progressive FBX shared control mesh limit");
            split.groups.emplace(&channel,std::pair{groupId,static_cast<std::int32_t>(owners.size())});
        }
    }
    split.consumedTargets=plan.consumedTargets;
    const auto defaults=session.evaluateNamed({});const auto &names=session.definition().blendShapes;
    for(std::size_t i=0;i<names.size();++i)
    {
        const auto &owners=membership.at(names[i]);account(owners.size());
        for(const auto meshIndex:owners)
        {
            const auto &mesh=meshes[meshIndex];
            const auto found=assetDefaults[meshIndex].find(names[i]);
            const auto actual=found==assetDefaults[meshIndex].end()?0.:found->second,expected=defaults.blendShapes[i];
            if(std::abs(actual-expected)>1e-10*std::max({1.,std::abs(actual),std::abs(expected)}))
                throw std::invalid_argument("Progressive FBX rig and asset morph defaults disagree");
            if(mesh.topology)
            {
                const auto foundRecord=records[meshIndex].find(names[i]);
                if(foundRecord==records[meshIndex].end()||foundRecord->second!=1)
                    throw std::invalid_argument("Progressive FBX export cannot preserve multiple source records for one target");
            }
        }
    }
    return result;
}
} // namespace iiActorObject::object3d_detail
