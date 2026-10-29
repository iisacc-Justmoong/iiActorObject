#include "Object3DFbxSkin.h"
#include "Object3DFbxMetadata.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <unordered_set>

namespace iiActorObject::object3d_detail
{
using fbx_metadata::text;
using fbx_metadata::uniqueChild;
namespace
{
std::size_t numericArrayCount(const ufbx_dom_node *node)
{
    if(!node||node->values.count!=1||!node->values.data||!ufbx_dom_is_array(node))
        throw std::invalid_argument("Invalid FBX skin blend array");
    const auto &value=node->values[0];
    if(value.value_blob.size&&!value.value_blob.data)throw std::invalid_argument("Missing FBX skin blend array data");
    std::size_t width=0;
    switch(value.type)
    {
    case UFBX_DOM_VALUE_ARRAY_I32:width=sizeof(std::int32_t);break;
    case UFBX_DOM_VALUE_ARRAY_I64:width=sizeof(std::int64_t);break;
    case UFBX_DOM_VALUE_ARRAY_F32:width=sizeof(float);break;
    case UFBX_DOM_VALUE_ARRAY_F64:width=sizeof(double);break;
    default:throw std::invalid_argument("Invalid FBX skin blend array type");
    }
    if(value.value_blob.size%width)throw std::invalid_argument("Malformed FBX skin blend array");
    return value.value_blob.size/width;
}
template<class List> bool missingData(const List &list)
{return list.count!=0&&!list.data;}
} // namespace
FbxSkinData readFbxSkin(const ufbx_mesh &source,const ufbx_node &node,
    const std::unordered_map<const ufbx_node *,BoneIndex> &bones,std::size_t maximumVertices)
{
    if(maximumVertices==0||source.num_vertices>maximumVertices)
        throw std::length_error("FBX skin vertex limit");
    if(source.skin_deformers.count>1||missingData(source.skin_deformers))
        throw std::invalid_argument("Invalid FBX skin deformer list");
    FbxSkinData result;result.weights.resize(source.num_vertices);
    if(source.skin_deformers.count)
    {
        const auto *skinPtr=source.skin_deformers[0];
        if(!skinPtr)throw std::invalid_argument("Missing FBX skin deformer");
        const auto &skin=*skinPtr;
        if(skin.skinning_method!=UFBX_SKINNING_METHOD_LINEAR&&
           skin.skinning_method!=UFBX_SKINNING_METHOD_DUAL_QUATERNION&&
           skin.skinning_method!=UFBX_SKINNING_METHOD_BLENDED_DQ_LINEAR)
            throw std::invalid_argument("Unsupported FBX skinning method");
        if(missingData(skin.vertices)||missingData(skin.weights)||missingData(skin.clusters)||
           missingData(skin.dq_vertices)||missingData(skin.dq_weights)||
           skin.num_dq_weights>skin.dq_vertices.count||skin.num_dq_weights>skin.dq_weights.count)
            throw std::invalid_argument("Invalid FBX skin array bounds");
        if(const auto *type=uniqueChild(skin.element.dom_node,"SkinningType"))
        {
            const auto name=text(type);
            if(name!="Linear"&&name!="DualQuaternion"&&name!="Blend")
                throw std::invalid_argument("Unsupported FBX skinning type");
        }
        const auto *indices=uniqueChild(skin.element.dom_node,"Indexes");
        const auto *blends=uniqueChild(skin.element.dom_node,"BlendWeights");
        if((indices!=nullptr)!=(blends!=nullptr))throw std::invalid_argument("Incomplete FBX skin blend arrays");
        if(indices&&(numericArrayCount(indices)!=numericArrayCount(blends)||numericArrayCount(indices)!=skin.num_dq_weights))
            throw std::invalid_argument("Mismatched FBX skin blend arrays");
        if(skin.skinning_method==UFBX_SKINNING_METHOD_DUAL_QUATERNION)result.profile.method=SkinningMethod::DualQuaternion;
        if(skin.skinning_method==UFBX_SKINNING_METHOD_BLENDED_DQ_LINEAR)
        {
            result.profile.method=SkinningMethod::Blended;result.profile.blendWeights.resize(source.num_vertices);
        }
        std::unordered_set<std::uint32_t> blendPoints;
        for(std::size_t i=0;i<skin.num_dq_weights;++i)
        {
            const auto point=skin.dq_vertices[i];const double amount=skin.dq_weights[i];
            if(point>=source.num_vertices||!blendPoints.insert(point).second||!std::isfinite(amount)||amount<0||amount>1)
                throw std::invalid_argument("Invalid FBX skin blend weight");
            if(result.profile.method==SkinningMethod::Blended)result.profile.blendWeights[point]=amount;
            else if((result.profile.method==SkinningMethod::DualQuaternion&&amount!=1)||
                    (result.profile.method==SkinningMethod::Linear&&amount!=0))
                throw std::invalid_argument("FBX skin blend weights contradict the skinning method");
        }
        for(std::size_t v=0;v<std::min(source.num_vertices,skin.vertices.count);++v)
        {
            std::map<BoneIndex,double> sums;const auto row=skin.vertices[v];
            if(row.weight_begin>skin.weights.count||row.num_weights>skin.weights.count-row.weight_begin)
                throw std::invalid_argument("Invalid FBX skin vertex weights");
            for(std::size_t j=0;j<row.num_weights;++j)
            {
                const auto weight=skin.weights[row.weight_begin+j];
                if(weight.cluster_index>=skin.clusters.count||!skin.clusters[weight.cluster_index])
                    throw std::invalid_argument("Invalid FBX skin cluster index");
                sums[bones.at(skin.clusters[weight.cluster_index]->bone_node)]+=weight.weight;
            }
            for(const auto [bone,value]:sums)if(value!=0)result.weights[v].push_back({bone,value});
        }
    }
    else
    {
        BoneIndex rigid=0;std::unordered_set<const ufbx_node *> seen;
        for(auto *p=&node;p;p=p->parent)
        {
            if(!seen.insert(p).second)throw std::invalid_argument("FBX rigid parent cycle");
            if(const auto i=bones.find(p);i!=bones.end()){rigid=i->second;break;}
        }
        for(auto &weights:result.weights)weights.push_back({rigid,1});
    }
    return result;
}
} // namespace iiActorObject::object3d_detail
