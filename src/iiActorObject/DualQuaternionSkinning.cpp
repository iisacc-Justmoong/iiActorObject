#include "DualQuaternionSkinning.h"
#include "CharacterMath.h"
#include "SkinningPalette.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace iiActorObject
{
using namespace character_detail;
std::vector<double> expandDualQuaternionWeights(const MeshTopology &topology,std::span<const double> pointWeights)
{
    if(!topology.layout||pointWeights.size()!=topology.points.size()||
       topology.layout->definition().positionCount!=topology.points.size())
        throw std::invalid_argument("Dual quaternion authoring weights must match source points");
    for(const auto value:pointWeights)weight(value);
    const auto &definition=topology.layout->definition();std::vector<double> result;
    result.reserve(definition.renderToLayout.size());
    for(const auto layout:definition.renderToLayout)
    {
        const auto point=definition.layouts.at(layout).position;
        if(point>=pointWeights.size())throw std::out_of_range("Dual quaternion source point index");
        result.push_back(pointWeights[point]);
    }
    return result;
}
void deformDualQuaternionSkin(const SkeletalMesh &mesh,const Pose &pose,std::size_t lodIndex,
    std::span<const double> morphWeights,std::span<const double> dualQuaternionWeights,
    std::span<DeformedVertex> destination)
{
    if(pose.skeleton().get()!=mesh.skeleton().get())throw std::invalid_argument("Mesh pose skeleton identity mismatch");
    if(lodIndex>=mesh.lods().size())throw std::out_of_range("Mesh LOD");
    const auto &lod=mesh.lods()[lodIndex];
    if(destination.size()!=lod.vertices.size()||dualQuaternionWeights.size()!=lod.vertices.size()||
       morphWeights.size()!=lod.morphTargets.size())throw std::invalid_argument("Deformation buffers must match LOD");
    for(const auto value:morphWeights)if(!std::isfinite(value))throw std::invalid_argument("Morph weight must be finite");
    for(const auto value:dualQuaternionWeights)weight(value);
    const auto skin=pose.skinMatrices(),normals=pose.normalMatrices();
    object3d_detail::DualQuaternionPalette palette(skin);
    std::vector<DeformedVertex> result;result.reserve(lod.vertices.size());
    for(const auto &vertex:lod.vertices)result.push_back({vertex.position,vertex.normal});
    for(std::size_t i=0;i<morphWeights.size();++i)
    {
        const auto amount=morphWeights[i];if(amount==0)continue;
        for(const auto &delta:lod.morphTargets[i].deltas)
        {
            auto &value=result[delta.vertex];
            value.position=add(value.position,mul(delta.position,amount));
            value.normal=add(value.normal,mul(delta.normal,amount));
        }
    }
    for(std::size_t i=0;i<result.size();++i)
    {
        const auto source=result[i];const auto amount=dualQuaternionWeights[i];
        Vector3 linearPosition{},linearNormal{};
        if(amount<1)
            for(const auto &influence:lod.vertices[i].weights)if(influence.weight!=0)
            {
                linearPosition=add(linearPosition,mul(skin[influence.bone].transformPoint(source.position),influence.weight));
                linearNormal=add(linearNormal,mul(normals[influence.bone].transformVector(source.normal),influence.weight));
            }
        Vector3 position=linearPosition,normal=linearNormal;
        if(amount>0)
        {
            const auto matrices=palette.blend(lod.vertices[i].weights);
            const auto dqPosition=matrices.position.transformPoint(source.position),dqNormal=matrices.normal.transformVector(source.normal);
            if(amount==1){position=dqPosition;normal=dqNormal;}
            else{position=add(mul(linearPosition,1-amount),mul(dqPosition,amount));
                 normal=add(mul(linearNormal,1-amount),mul(dqNormal,amount));}
        }
        if(!finite(position))throw std::overflow_error("Skin deformation overflow");
        result[i]={position,unit(normal)};
    }
    std::copy(result.begin(),result.end(),destination.begin());
}
} // namespace iiActorObject
