#include "Object3DFbxMorphChannelsWrite.h"
#include "Object3DMorphChannels.h"
#include "Object3DFbxMorphWrite.h"
#include "Object3DFbxSceneNodes.h"
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail
{
using namespace fbx_binary;
using namespace fbx_scene_nodes;

FbxMorphRecords collectFbxMorphRecords(const Object3DMesh &mesh,bool groupLogicalChannels)
{
    FbxMorphRecords result;
    if(groupLogicalChannels)
    {
        for(auto &group:logicalMorphGroups(mesh))
        {result.projectedRecordCounts.push_back(group.recordCount);result.morphs.push_back(std::move(group.morph));}
    }
    else if(mesh.topology)result.morphs=mesh.topology->morphTargets;
    else for(const auto &m:mesh.geometry.morphTargets)
    {
        TopologyMorph target;target.name=m.name;target.normalDeltas.emplace();
        for(const auto &v:m.deltas)
        {
            target.deltas.push_back({static_cast<std::uint32_t>(v.vertex),v.position});
            if(v.normal!=Vector3{})target.normalDeltas->push_back({static_cast<std::uint32_t>(v.vertex),v.normal});
        }
        result.morphs.push_back(std::move(target));
    }
    return result;
}

void writeFbxMorphChannels(const FbxMorphChannelWriteInput &input,FbxMorphChannelWriteState &state)
{
    const auto &mesh=input.mesh;const auto &morphs=input.records.morphs;
    const auto &projectedRecords=input.records.projectedRecordCounts;
    if(!projectedRecords.empty()&&projectedRecords.size()!=morphs.size())
        throw std::invalid_argument("FBX morph record counts differ from channels");
    const auto meshIndex=input.meshIndex;const auto geometry=input.geometry;
    const auto *progressive=input.progressive;
    if(progressive&&(meshIndex>=progressive->channels.size()||meshIndex>=progressive->consumedTargets.size()))
        throw std::invalid_argument("FBX progressive mesh plan is incomplete");
    if(progressive)for(const auto *channel:progressive->channels[meshIndex])
        if(!channel)throw std::invalid_argument("FBX progressive mesh plan contains a null channel");
    if(progressive&&input.includeAnimation&&!progressive->channels[meshIndex].empty()&&!input.curveName)
        throw std::invalid_argument("FBX progressive curve naming is unavailable");
    const auto *source=mesh.topology?&*mesh.topology:nullptr;
    if(source&&!morphs.empty()&&!source->layout)throw std::invalid_argument("Invalid FBX morph source topology");
    const auto count=source?source->points.size():mesh.geometry.vertices.size();
    auto &next=state.nextId;auto &objects=state.objects;auto &connections=state.connections;
    auto &morphChannels=state.morphChannels;auto &progressiveChannels=state.progressiveChannels;
    auto &progressiveShapeWork=state.progressiveShapeWork;
    const auto connect=[&](std::int64_t child,std::int64_t parent)
    {connections.children.push_back({"C",{prop("OO"),prop(child),prop(parent)},{}});};
    std::map<std::string_view,std::size_t,std::less<>> logicalMorphIndices;
    for(std::size_t target=0;target<mesh.geometry.morphTargets.size();++target)
        logicalMorphIndices.emplace(mesh.geometry.morphTargets[target].name,target);
    if (!morphs.empty())
    {
        std::map<std::string_view,double,std::less<>> defaults;
        for(const auto &entry:mesh.morphDefaults)defaults.emplace(entry.name,entry.value);
        const auto blend = next++;
        objects.children.push_back(
            {"Deformer",
             {prop(blend), prop(objectName(mesh.name + "Shapes", "Deformer")), prop("BlendShape")},
             {value("Version", prop(100))}});
        connect(blend, geometry);
        std::set<std::string> usedNames;
        if(progressive)for(const auto *channel:progressive->channels[meshIndex])usedNames.insert(channel->name);
        for (std::size_t morphIndex=0;morphIndex<morphs.size();++morphIndex)
        {
            const auto &m=morphs[morphIndex];
            if(progressive&&progressive->consumedTargets[meshIndex].contains(m.name))continue;
            auto name=m.name;for(std::size_t suffix=1;!usedNames.insert(name).second;++suffix)name=m.name+"_Source"+std::to_string(suffix);
            const auto channel = next++, shape = next++;
            if(input.includeAnimation)
            {
                const auto logical=logicalMorphIndices.find(m.name);
                if(logical==logicalMorphIndices.end())throw std::logic_error("FBX morph lacks its logical target");
                morphChannels[{meshIndex,logical->second}].push_back(channel);
            }
            const auto payload=object3d_detail::encodeFbxMorph(m,source,count,input.allowMorphNormalMetadata,false,progressiveShapeWork);
            const auto foundDefault=defaults.find(m.name);
            const auto defaultPercent=foundDefault==defaults.end()?0.:100.*foundDefault->second;
            if(!std::isfinite(defaultPercent))throw std::overflow_error("FBX default morph percentage overflow");
            objects.children.push_back(
                {"Deformer",
                 {prop(channel), prop(objectName(name, "SubDeformer")), prop("BlendShapeChannel")},
                 {value("Version", prop(100)), value("DeformPercent", prop(defaultPercent)),
                  value("FullWeights", array(std::array<double, 1>{100.0})),
                  value("iiSourceName",prop(m.name)),value("iiSourceTarget",prop(static_cast<std::int64_t>(m.sourceTarget))),
                  value("iiNormalMode",prop(m.normalDeltas?1:0)),value("iiNormalProjectionExact",prop(payload.exactNormals?1:0)),
                  value("iiLayoutNormals",blob(payload.normalMetadata))}});
            if(!projectedRecords.empty()&&projectedRecords[morphIndex]>1)
                objects.children.back().children.push_back(value("iiActorObjectMergedMorphRecords",prop(static_cast<std::int64_t>(projectedRecords[morphIndex]))));
            objects.children.push_back(object3d_detail::makeFbxMorphShape(payload,name,shape));
            connect(shape, channel);
            connect(channel, blend);
        }
        std::map<std::string_view,const TopologyMorph *,std::less<>> progressiveMorphs;
        if(progressive)for(const auto &m:morphs)
            if(progressive->consumedTargets[meshIndex].contains(m.name))progressiveMorphs.emplace(m.name,&m);
        if(progressive)for(const auto *program:progressive->channels[meshIndex])
        {
            const auto channel=next++;std::vector<double> weights;weights.reserve(program->keys.size());
            for(const auto &key:program->keys)weights.push_back(100.*key.fullWeight);
            objects.children.push_back({"Deformer",
                {prop(channel),prop(objectName(program->name,"SubDeformer")),prop("BlendShapeChannel")},
                {value("Version",prop(100)),value("DeformPercent",prop(100.*program->defaultValue)),
                 value("FullWeights",array(weights)),value("iiActorObjectProgressive",prop(1))}});
            if(const auto group=progressive->groups.find(program);group!=progressive->groups.end())
            {
                objects.children.back().children.push_back(value("iiActorObjectProgressiveGroup",prop(group->second.first)));
                objects.children.back().children.push_back(value("iiActorObjectProgressiveGroupSize",prop(group->second.second)));
            }
            if(progressive->sparse.contains(program))
            {
                std::vector<std::int32_t> presence;presence.reserve(program->keys.size());
                for(const auto &key:program->keys)presence.push_back(progressiveMorphs.contains(key.target)?1:0);
                objects.children.back().children.push_back(value("iiActorObjectProgressiveSparse",prop(1)));
                objects.children.back().children.push_back(value("iiActorObjectProgressivePresence",packed(presence)));
            }
            if(input.includeAnimation)progressiveChannels[input.curveName(program->name)].push_back(channel);
            connect(channel,blend);
            for(const auto &key:program->keys)
            {
                const auto found=progressiveMorphs.find(key.target);
                if(found==progressiveMorphs.end()&&!progressive->sparse.contains(program))
                    throw std::logic_error("Progressive FBX target binding changed");
                TopologyMorph morph;
                if(found==progressiveMorphs.end())morph.name=key.target;
                else morph=*found->second;
                if(found!=progressiveMorphs.end()&&source&&!morph.normalDeltas)
                {
                    constexpr std::size_t limit=128'000'000;
                    const auto account=[&](std::size_t count)
                    {if(progressiveShapeWork>limit||count>limit-progressiveShapeWork)throw std::length_error("Progressive FBX derived normal budget");progressiveShapeWork+=count;};
                    account(source->points.size());account(source->layout->definition().layouts.size());
                    account(source->layout->triangles().size());
                    morph.normalDeltas=topologyMorphNormalDeltas(*source,morph);
                }
                const auto payload=object3d_detail::encodeFbxMorph(morph,source,count,input.allowMorphNormalMetadata,true,progressiveShapeWork);const auto shape=next++;
                objects.children.push_back(object3d_detail::makeFbxMorphShape(payload,key.sourceName.empty()?key.target:key.sourceName,shape));
                connect(shape,channel);
            }
        }
    }
}
} // namespace iiActorObject::object3d_detail
