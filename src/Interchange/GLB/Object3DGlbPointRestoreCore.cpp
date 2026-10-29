#include "Object3DGlbPointRestoreCore.h"
#include "Object3DGlbPointIdentity.h"
#include "CharacterMath.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail
{
using namespace character_detail;
namespace
{
bool zero(Vector3 value){return value.x==0&&value.y==0&&value.z==0;}
bool samePointWeights(const std::vector<SkinWeight> &a,const std::vector<SkinWeight> &b)
{
    if(a.size()!=b.size())return false;
    return std::all_of(a.begin(),a.end(),[&](const auto &weight)
    {
        return std::any_of(b.begin(),b.end(),[&](const auto &other)
            {return other.bone==weight.bone&&other.weight==weight.weight;});
    });
}
Vector3 point(const std::array<double,12> &m,Vector3 v)
{return {m[0]*v.x+m[3]*v.y+m[6]*v.z+m[9],m[1]*v.x+m[4]*v.y+m[7]*v.z+m[10],m[2]*v.x+m[5]*v.y+m[8]*v.z+m[11]};}
Vector3 direction(const std::array<double,12> &m,Vector3 v)
{return {m[0]*v.x+m[3]*v.y+m[6]*v.z,m[1]*v.x+m[4]*v.y+m[7]*v.z,m[2]*v.x+m[5]*v.y+m[8]*v.z};}
}

GlbPointRestorePlan planGlbPointRestore(const GlbPointIdentity &saved,
    const MeshTopology &topology,const MeshTopologyDefinition &definition,
    std::span<const std::uint32_t> ids,const GlbPointRestoreOptions &options)
{
    if(!saved.pointCount||saved.pointCount>options.maximumPoints)
        throw std::length_error("GLB point identity point budget");
    if(ids.size()!=topology.points.size()||saved.targetCount!=saved.targetNames.size()||
       saved.targetNames.size()!=topology.morphTargets.size())
        throw std::invalid_argument("Invalid point identity dimensions");
    for(std::size_t i=0;i<saved.targetNames.size();++i)
        if(saved.targetNames[i]!=topology.morphTargets[i].name)throw std::invalid_argument("Stale point identity morph names");
    if(options.skinned)
    {
        if(options.sidecarToBone.empty()||options.sidecarToBone.size()!=saved.boneNames.size())
            throw std::invalid_argument("Invalid point identity skin mapping");
    }
    else if(!saved.boneNames.empty()||!saved.emptyWeights.empty())
        throw std::invalid_argument("Stale point identity skin binding");

    // Indexed instances take precedence over unused accessor rows for the same ID.
    std::vector<bool> visible(topology.points.size()),idHasVisible(saved.pointCount);
    for(const auto id:ids)if(id>=saved.pointCount)throw std::invalid_argument("GLB point identity out of range");
    for(const auto &face:definition.polygons)for(const auto layout:face)
    {
        const auto index=definition.layouts.at(layout).position;
        visible.at(index)=true;idHasVisible.at(ids[index])=true;
    }
    GlbPointRestorePlan result;
    result.points.resize(saved.pointCount);std::vector<bool> occupied(saved.pointCount);
    for(std::size_t i=0;i<ids.size();++i)
    {
        const auto id=ids[i];if(idHasVisible[id]&&!visible[i])continue;
        if(occupied[id])
        {
            if(result.points[id].position!=topology.points[i].position||!samePointWeights(result.points[id].weights,topology.points[i].weights))
                throw std::invalid_argument("GLB point identity attributes conflict");
        }
        else{result.points[id]=topology.points[i];occupied[id]=true;}
    }
    std::vector<bool> unused(saved.pointCount);
    for(const auto &record:saved.unused)
    {
        if(record.id>=saved.pointCount||occupied[record.id])throw std::invalid_argument("GLB point identity unused point became visible");
        TopologyControlPoint restored;restored.position=point(options.affine,record.position);
        if(!finite(restored.position))throw std::invalid_argument("GLB point identity transform overflow");
        if(options.skinned)
        {
            for(const auto &weight:record.weights)
            {
                if(weight.bone>=options.sidecarToBone.size())throw std::invalid_argument("Invalid GLB point identity weight joint");
                restored.weights.push_back({options.sidecarToBone[weight.bone],weight.weight});
            }
        }
        else if(options.rigidBone!=NoBone)
        {
            if(!record.weights.empty())throw std::invalid_argument("Stale rigid point identity weight");
            restored.weights.push_back({options.rigidBone,1});
        }
        else
        {
            for(const auto &weight:record.weights)
            {if(weight.bone!=0)throw std::invalid_argument("Invalid unskinned point identity weight");restored.weights.push_back(weight);}
        }
        result.points[record.id]=std::move(restored);occupied[record.id]=true;unused[record.id]=true;
    }
    if(std::find(occupied.begin(),occupied.end(),false)!=occupied.end())
        throw std::invalid_argument("Missing GLB source point identity");
    if(options.skinned)
    {
        std::vector<bool> seen(saved.pointCount);
        for(const auto id:saved.emptyWeights)
        {
            if(id>=saved.pointCount||seen[id])throw std::invalid_argument("Invalid GLB empty source weight identity");
            seen[id]=true;auto &weights=result.points[id].weights;
            if(unused[id]){if(!weights.empty())throw std::invalid_argument("Conflicting GLB empty source weights");}
            else if(weights.size()==1&&weights[0].bone==options.sidecarToBone[0]&&weights[0].weight==1)weights.clear();
        }
    }
    std::vector<std::vector<const GlbUnusedPointMorph*>> unusedMorphs(saved.targetCount);
    for(const auto &entry:saved.morphs)
    {
        if(entry.target>=saved.targetCount||entry.point>=saved.pointCount||!unused[entry.point])
            throw std::invalid_argument("Stale GLB unused morph point");
        unusedMorphs[entry.target].push_back(&entry);
    }
    result.morphs.resize(saved.targetCount);
    std::vector<Vector3> oldDeltas,values;
    std::vector<std::uint32_t> epoch,touched;std::size_t totalDeltas=0;
    if(saved.targetCount)
    {oldDeltas.resize(topology.points.size());values.resize(saved.pointCount);epoch.resize(saved.pointCount);}
    for(std::size_t target=0;target<saved.targetCount;++target)
    {
        std::fill(oldDeltas.begin(),oldDeltas.end(),Vector3{});touched.clear();
        for(const auto &delta:topology.morphTargets[target].deltas)oldDeltas.at(delta.point)=delta.position;
        const auto tag=static_cast<std::uint32_t>(target+1);
        for(std::size_t i=0;i<ids.size();++i)
        {
            const auto id=ids[i];if(idHasVisible[id]&&!visible[i])continue;
            if(epoch[id]!=tag){epoch[id]=tag;values[id]=oldDeltas[i];touched.push_back(id);}
            else if(values[id]!=oldDeltas[i])throw std::invalid_argument("GLB point identity morph conflict");
        }
        for(const auto *entry:unusedMorphs[target])
        {
            const auto id=entry->point;
            if(epoch[id]!=tag){epoch[id]=tag;values[id]={};touched.push_back(id);}
            values[id]=add(values[id],direction(options.affine,entry->delta));
            if(!finite(values[id]))throw std::invalid_argument("GLB point identity morph overflow");
        }
        std::sort(touched.begin(),touched.end());
        for(const auto id:touched)if(!zero(values[id]))
        {
            if(totalDeltas==options.maximumMorphDeltas)throw std::length_error("GLB point identity morph budget");
            result.morphs[target].push_back({id,values[id]});++totalDeltas;
        }
    }
    result.layoutPoints.reserve(definition.layouts.size());
    for(const auto &layout:definition.layouts)
    {
        if(layout.position>=ids.size())throw std::out_of_range("Invalid GLB point layout");
        result.layoutPoints.push_back(ids[layout.position]);
    }
    return result;
}
}
