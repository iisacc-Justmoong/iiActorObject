#include "Object3DMorphChannels.h"
#include "CharacterMath.h"
#include <map>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail
{
using namespace character_detail;
namespace
{
void accumulate(std::map<std::uint32_t,Vector3> &values,std::uint32_t index,Vector3 delta)
{
    auto &value=values[index];value=add(value,delta);
    if(!finite(value))throw std::overflow_error("Logical morph delta overflow");
}
}
std::vector<LogicalMorphGroup> logicalMorphGroups(const Object3DMesh &mesh,std::span<const TopologyMorph> records)
{
    std::map<std::string,std::size_t> byName;
    std::vector<std::vector<std::size_t>> members;
    for(std::size_t i=0;i<records.size();++i)
    {
        const auto [it,added]=byName.emplace(records[i].name,members.size());
        if(added)members.emplace_back();members[it->second].push_back(i);
    }
    std::vector<LogicalMorphGroup> result;result.reserve(members.size());
    for(const auto &group:members)
    {
        if(group.size()==1){result.push_back({records[group.front()],group.front(),1});continue;}
        TopologyMorph combined;combined.name=records[group.front()].name;
        std::map<std::uint32_t,Vector3> positions,derived,explicitNormals;
        bool hasExplicit=false;
        for(const auto index:group)
        {
            const auto &record=records[index];
            for(const auto &v:record.deltas)
            {
                accumulate(positions,v.point,v.position);
                if(!record.normalDeltas)accumulate(derived,v.point,v.position);
            }
            if(record.normalDeltas)
            {
                hasExplicit=true;
                for(const auto &v:*record.normalDeltas)accumulate(explicitNormals,v.layout,v.normal);
            }
        }
        for(const auto &[point,value]:positions)if(value!=Vector3{})combined.deltas.push_back({point,value});
        if(hasExplicit)
        {
            if(!derived.empty())
            {
                if(!mesh.topology)throw std::invalid_argument("Derived render morph normals require source topology");
                TopologyMorph derivedMorph;derivedMorph.name=combined.name;
                for(const auto &[point,value]:derived)if(value!=Vector3{})derivedMorph.deltas.push_back({point,value});
                for(const auto &v:topologyMorphNormalDeltas(*mesh.topology,derivedMorph))
                    accumulate(explicitNormals,v.layout,v.normal);
            }
            combined.normalDeltas.emplace();
            for(const auto &[layout,value]:explicitNormals)if(value!=Vector3{})combined.normalDeltas->push_back({layout,value});
        }
        result.push_back({std::move(combined),group.front(),group.size()});
    }
    return result;
}
std::vector<LogicalMorphGroup> logicalMorphGroups(const Object3DMesh &mesh)
{
    if(mesh.topology)return logicalMorphGroups(mesh,mesh.topology->morphTargets);
    std::vector<TopologyMorph> records;
    for(const auto &source:mesh.geometry.morphTargets)
    {
        TopologyMorph record;record.name=source.name;record.normalDeltas.emplace();
        for(const auto &delta:source.deltas)
        {
            if(delta.vertex>=NoTopologyIndex)throw std::length_error("Render morph index limit");
            const auto index=static_cast<std::uint32_t>(delta.vertex);
            record.deltas.push_back({index,delta.position});
            if(delta.normal!=Vector3{})record.normalDeltas->push_back({index,delta.normal});
        }
        records.push_back(std::move(record));
    }
    return logicalMorphGroups(mesh,records);
}
} // namespace iiActorObject::object3d_detail
