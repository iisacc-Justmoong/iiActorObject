#include "Object3DFbxMorphWrite.h"
#include "CharacterMath.h"
#include "Object3DFbxSceneNodes.h"
#include <bit>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>

namespace iiActorObject::object3d_detail
{
using namespace fbx_binary;
namespace
{
void xyz(std::vector<double> &values,Vector3 value)
{values.insert(values.end(),{value.x,value.y,value.z});}
}
FbxMorphPayload encodeFbxMorph(const TopologyMorph &morph,const MeshTopology *source,
    std::size_t pointCount,bool allowInexactNormals,bool progressiveShape,std::size_t &progressiveWork)
{
    if(pointCount>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        throw std::length_error("FBX morph point index limit");
    if(source&&(source->points.size()!=pointCount||!source->layout))
        throw std::invalid_argument("Invalid FBX morph source topology");
    if(progressiveShape)
    {
        constexpr std::size_t limit=128'000'000;
        const auto account=[&](std::size_t count)
        {if(progressiveWork>limit||count>limit-progressiveWork)throw std::length_error("Progressive FBX shape processing budget");progressiveWork+=count;};
        account(morph.deltas.size());
        if(morph.normalDeltas){account(morph.normalDeltas->size());if(source)account(source->layout->definition().layouts.size());}
    }
    FbxMorphPayload result;result.explicitNormals=morph.normalDeltas.has_value();
    std::map<std::uint32_t,Vector3> positions,pointNormals;
    for(const auto &delta:morph.deltas)
        if(delta.point>=pointCount||!character_detail::finite(delta.position)||!positions.emplace(delta.point,delta.position).second)
            throw std::invalid_argument("Invalid FBX morph point delta");
    if(morph.normalDeltas)
    {
        std::map<std::uint32_t,Vector3> layoutNormals;
        const auto layoutCount=source?source->layout->definition().layouts.size():pointCount;
        for(const auto &delta:*morph.normalDeltas)
            if(delta.layout>=layoutCount||!character_detail::finite(delta.normal)||!layoutNormals.emplace(delta.layout,delta.normal).second)
                throw std::invalid_argument("Invalid FBX morph layout normal");
        if(source)
        {
            const auto &layouts=source->layout->definition().layouts;
            for(std::uint32_t i=0;i<layouts.size();++i)
            {
                const auto found=layoutNormals.find(i);auto value=found==layoutNormals.end()?Vector3{}:found->second;
                const auto normalId=layouts[i].normal;
                if(normalId!=NoTopologyIndex)
                {
                    const auto base=source->normals.at(normalId);const auto length=std::hypot(base.x,base.y,base.z);
                    if(!std::isfinite(length))throw std::invalid_argument("Nonfinite FBX morph base normal");
                    const auto scale=length>0?length:1;
                    value={value.x*scale,value.y*scale,value.z*scale};
                    if(!character_detail::finite(value))throw std::invalid_argument("Nonfinite FBX morph normal projection");
                }
                const auto [it,added]=pointNormals.emplace(layouts[i].position,value);
                if(!added&&(std::abs(it->second.x-value.x)>1e-10||std::abs(it->second.y-value.y)>1e-10||std::abs(it->second.z-value.z)>1e-10))
                    result.exactNormals=false;
            }
            for(const auto &delta:*morph.normalDeltas)positions.try_emplace(layouts[delta.layout].position,Vector3{});
        }
        else pointNormals=std::move(layoutNormals);
        if(!result.exactNormals&&(progressiveShape||!allowInexactNormals))
            throw std::invalid_argument(progressiveShape?
                "Progressive FBX cannot preserve distinct per-layout morph normals":"FBX control-point normals cannot encode distinct layout morph normals; explicitly enable allowMorphNormalMetadata to retain the exact SDK values");
        for(const auto &[point,value]:pointNormals)if(value!=Vector3{})positions.try_emplace(point,Vector3{});
    }
    for(const auto &[point,value]:positions)
    {
        result.indices.push_back(static_cast<std::int32_t>(point));xyz(result.deltas,value);
        if(morph.normalDeltas)xyz(result.normalDeltas,pointNormals.contains(point)?pointNormals.at(point):Vector3{});
    }
    if(!progressiveShape&&morph.normalDeltas)for(const auto &delta:*morph.normalDeltas)
    {
        integer(result.normalMetadata,delta.layout,4);
        for(const auto value:{delta.normal.x,delta.normal.y,delta.normal.z})
            integer(result.normalMetadata,std::bit_cast<std::uint64_t>(value),8);
    }
    return result;
}
Node makeFbxMorphShape(const FbxMorphPayload &payload,std::string_view name,std::int64_t id)
{
    if(payload.indices.size()>std::numeric_limits<std::size_t>::max()/3||
       payload.deltas.size()!=payload.indices.size()*3||
       (payload.explicitNormals?payload.normalDeltas.size()!=payload.deltas.size():!payload.normalDeltas.empty()))
        throw std::invalid_argument("Invalid FBX morph payload dimensions");
    for(const auto index:payload.indices)if(index<0)throw std::invalid_argument("Invalid FBX morph payload index");
    Node result{"Geometry",{prop(id),prop(fbx_scene_nodes::objectName(name,"Geometry")),prop("Shape")},
        {value("Version",prop(100)),value("Indexes",array(payload.indices)),value("Vertices",array(payload.deltas))}};
    if(payload.explicitNormals)result.children.push_back(value("Normals",array(payload.normalDeltas)));
    return result;
}
} // namespace iiActorObject::object3d_detail
