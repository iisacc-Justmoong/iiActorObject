#include "Object3DFbxMorphShape.h"
#include "CharacterMath.h"
#include "Object3DFbxMetadata.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

namespace iiActorObject::object3d_detail
{
using character_detail::finite;
using character_detail::mul;
namespace
{
Vector3 vector(ufbx_vec3 value)
{return {value.x,value.y,value.z};}
template<class List> bool missingData(const List &list)
{return list.count&&!list.data;}
std::vector<TopologyNormalDelta> normalArray(const ufbx_dom_node *node,std::size_t limit)
{
    const auto &value=fbx_metadata::scalar(node);const auto bytes=value.value_blob;
    if(value.type!=UFBX_DOM_VALUE_STRING||bytes.size%28||bytes.size/28>limit||(!bytes.data&&bytes.size))
        throw std::invalid_argument("Invalid FBX morph normal metadata or limit");
    const auto *data=static_cast<const std::uint8_t*>(bytes.data);
    const auto read=[&](std::size_t offset,unsigned count)
    {std::uint64_t result=0;for(unsigned i=0;i<count;++i)result|=std::uint64_t(data[offset+i])<<(i*8);return result;};
    std::vector<TopologyNormalDelta> result;
    for(std::size_t i=0;i<bytes.size;i+=28)
    {
        const Vector3 n{std::bit_cast<double>(read(i+4,8)),std::bit_cast<double>(read(i+12,8)),std::bit_cast<double>(read(i+20,8))};
        if(!finite(n))throw std::invalid_argument("Nonfinite FBX morph normal metadata");
        result.push_back({static_cast<std::uint32_t>(read(i,4)),n});
    }
    return result;
}
}
FbxMorphShapeData readFbxMorphShape(const ufbx_blend_shape &shape,
    const ufbx_mesh &source,const MeshTopology &topology,const ufbx_matrix &geometry,
    const ufbx_dom_node *channelMetadata,bool allowNormalMetadata,ufbx_mirror_axis mirrorAxis)
{
    const auto count=shape.offset_vertices.count;
    if(count>topology.points.size()||shape.position_offsets.count!=count||
       (shape.normal_offsets.count&&shape.normal_offsets.count!=count)||
       (shape.offset_weights.count&&shape.offset_weights.count!=count)||
       missingData(shape.offset_vertices)||missingData(shape.position_offsets)||
       missingData(shape.normal_offsets)||missingData(shape.offset_weights))
        throw std::invalid_argument("Invalid FBX blend shape arrays");
    const auto normalMatrix=ufbx_matrix_for_normals(&geometry);
    const auto determinant=ufbx_matrix_determinant(&geometry);
    const auto normal=[&](ufbx_vec3 value)
    {
        auto result=vector(ufbx_transform_direction(&normalMatrix,value));
        if(determinant!=0)result=mul(result,1/std::abs(determinant));
        if(!finite(result))throw std::invalid_argument("Nonfinite FBX normal");
        return result;
    };
    FbxMorphShapeData result;result.deltas.reserve(count);
    std::set<std::uint32_t> points;
    for(std::size_t i=0;i<count;++i)
    {
        const auto point=shape.offset_vertices[i];
        if(point>=topology.points.size()||!points.insert(point).second)
            throw std::invalid_argument("FBX blend shape source point");
        const auto weight=shape.offset_weights.count?shape.offset_weights[i]:1;
        if(!std::isfinite(weight)||!finite(vector(shape.position_offsets[i]))||
           (shape.normal_offsets.count&&!finite(vector(shape.normal_offsets[i]))))
            throw std::invalid_argument("Nonfinite FBX blend shape value");
        auto delta=mul(vector(ufbx_transform_direction(&geometry,shape.position_offsets[i])),weight);
        if(!finite(delta))throw std::invalid_argument("Nonfinite FBX blend shape position");
        result.deltas.push_back({point,delta});
    }
    if(channelMetadata&&missingData(channelMetadata->children))
        throw std::invalid_argument("Missing FBX morph metadata children");
    const auto *modeNode=fbx_metadata::child(channelMetadata,"iiNormalMode");
    result.restoredNormalMetadata=allowNormalMetadata&&modeNode;
    if((shape.normal_offsets.count||result.restoredNormalMetadata)&&!topology.layout)
        throw std::invalid_argument("Missing FBX morph topology layout");
    if(result.restoredNormalMetadata)
    {
        const auto mode=fbx_metadata::number(modeNode);
        if(mode>1)throw std::invalid_argument("Invalid FBX morph normal mode");
        if(mode)
        {
            if(missingData(source.vertex_normal.values))throw std::invalid_argument("Missing FBX source normal data");
            result.normalDeltas=normalArray(fbx_metadata::child(channelMetadata,"iiLayoutNormals"),topology.maximumRenderMorphDeltas);
            std::set<std::uint32_t> normalLayouts;const auto &definition=topology.layout->definition();
            std::map<std::uint32_t,Vector3> rawLayoutNormals,expectedOffsets,actualOffsets;
            for(auto &delta:*result.normalDeltas)
            {
                // SDK metadata is not transformed by ufbx; standard arrays are.
                switch(mirrorAxis)
                {
                case UFBX_MIRROR_AXIS_X:delta.normal.x=-delta.normal.x;break;
                case UFBX_MIRROR_AXIS_Y:delta.normal.y=-delta.normal.y;break;
                case UFBX_MIRROR_AXIS_Z:delta.normal.z=-delta.normal.z;break;
                default:break;
                }
                rawLayoutNormals.emplace(delta.layout,delta.normal);
            }
            for(std::uint32_t i=0;i<definition.layouts.size();++i)
            {
                const auto index=definition.normalCount?definition.layouts[i].normal:i;
                if(index>=source.vertex_normal.values.count)throw std::invalid_argument("FBX morph metadata base normal is missing");
                const auto raw=vector(source.vertex_normal.values[index]);const auto size=std::hypot(raw.x,raw.y,raw.z);
                if(!std::isfinite(size))throw std::invalid_argument("Nonfinite FBX source normal");
                const auto found=rawLayoutNormals.find(i);const auto value=found==rawLayoutNormals.end()?Vector3{}:found->second;
                const auto expected=mul(value,size>0?size:1);
                if(!finite(expected))throw std::invalid_argument("Nonfinite FBX morph normal projection");
                expectedOffsets.try_emplace(definition.layouts[i].position,expected);
            }
            for(std::size_t i=0;i<shape.normal_offsets.count;++i)
            {
                auto value=vector(shape.normal_offsets[i]);if(shape.offset_weights.count)value=mul(value,shape.offset_weights[i]);
                if(!finite(value))throw std::invalid_argument("Nonfinite FBX morph normal projection");
                actualOffsets.emplace(shape.offset_vertices[i],value);
            }
            for(const auto &[point,value]:actualOffsets){(void)value;expectedOffsets.try_emplace(point,Vector3{});}
            for(const auto &[point,expected]:expectedOffsets)
            {
                const auto found=actualOffsets.find(point);const auto actual=found==actualOffsets.end()?Vector3{}:found->second;
                const auto tolerance=1e-8*std::max({1.0,std::hypot(expected.x,expected.y,expected.z),std::hypot(actual.x,actual.y,actual.z)});
                if(std::hypot(expected.x-actual.x,expected.y-actual.y,expected.z-actual.z)>tolerance)
                    throw std::invalid_argument("FBX morph normal metadata is stale; disable restoration to import current standard normals");
            }
            for(auto &delta:*result.normalDeltas)
            {
                if(delta.layout>=definition.layouts.size()||!normalLayouts.insert(delta.layout).second)
                    throw std::invalid_argument("Invalid FBX morph normal layout");
                const auto index=definition.normalCount?definition.layouts[delta.layout].normal:delta.layout;
                if(index>=source.vertex_normal.values.count)throw std::invalid_argument("FBX morph metadata base normal is missing");
                const auto raw=vector(source.vertex_normal.values[index]);const auto length=std::hypot(raw.x,raw.y,raw.z);
                ufbx_vec3 base{};if(length>0){base.x=raw.x/length;base.y=raw.y/length;base.z=raw.z/length;}
                const auto transformed=length>0?normal(base):Vector3{};
                const auto scale=length>0?std::hypot(transformed.x,transformed.y,transformed.z):1;
                if(!(scale>0)||!std::isfinite(scale))throw std::invalid_argument("Singular FBX morph normal transform");
                ufbx_vec3 value{};value.x=delta.normal.x;value.y=delta.normal.y;value.z=delta.normal.z;
                delta.normal=mul(normal(value),1/scale);
                if(!finite(delta.normal))throw std::invalid_argument("Nonfinite FBX morph normal delta");
            }
        }
    }
    else if(shape.normal_offsets.count)
    {
        result.normalDeltas.emplace();std::map<std::uint32_t,Vector3> offsets;
        for(std::size_t i=0;i<count;++i)
        {
            auto value=normal(shape.normal_offsets[i]);if(shape.offset_weights.count)value=mul(value,shape.offset_weights[i]);
            if(!finite(value))throw std::invalid_argument("Nonfinite FBX morph normal projection");
            offsets.emplace(shape.offset_vertices[i],value);
        }
        const auto &definition=topology.layout->definition();
        for(std::uint32_t i=0;i<definition.layouts.size();++i)
        {
            const auto &layout=definition.layouts[i];const auto found=offsets.find(layout.position);
            if(found==offsets.end())continue;
            if(layout.normal>=topology.normals.size())throw std::invalid_argument("FBX morph base normal is missing");
            const auto base=topology.normals[layout.normal];const auto scale=std::hypot(base.x,base.y,base.z);
            if(!(scale>0)||!std::isfinite(scale))throw std::invalid_argument("Singular FBX morph normal transform");
            const auto delta=mul(found->second,1/scale);
            if(!finite(delta))throw std::invalid_argument("Nonfinite FBX morph normal delta");
            if(delta!=Vector3{})result.normalDeltas->push_back({i,delta});
        }
    }
    return result;
}
} // namespace iiActorObject::object3d_detail
