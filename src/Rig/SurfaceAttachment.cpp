#include "SurfaceAttachment.h"
#include "SurfaceBindingView.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <utility>

namespace iiActorObject
{
using surface_detail::MeshView;
SkinTransferResult transferMeshBindingWeights(Object3DData &data,const SurfaceBinding &binding,std::size_t maximumInfluences,BoneIndex fallback)
{return transferMeshBindingWeights(data,binding,maximumInfluences,fallback,SkinningProfileTransfer::PreserveTarget);}
static SkinTransferResult transferWeights(Object3DData &data,const SurfaceBinding &binding,std::size_t maximumInfluences,
    BoneIndex fallback,SkinningProfileTransfer policy,const SurfaceBindingBasis *basis)
{
    if(basis){const std::optional<SurfaceBindingBasis> entry=*basis;validateMeshSurfaceBindingsWithBases(data,std::span(&binding,1),std::span(&entry,1));}
    else validateMeshSurfaceBindings(data,std::span(&binding,1));
    if(maximumInfluences>32||(fallback!=NoBone&&fallback>=data.bones.size()))throw std::invalid_argument("Invalid binding skin options");
    auto next=data;const MeshView source{data.meshes[binding.sourceMesh]};auto &target=next.meshes[binding.targetMesh];SkinTransferResult result;
    const auto &sourceProfile=source.mesh.topology?source.mesh.topology->skinning:source.mesh.geometry.skinning;
    auto &targetProfile=target.topology?target.topology->skinning:target.geometry.skinning;
    const auto targetCount=target.topology?target.topology->points.size():target.geometry.vertices.size();
    const bool copyProfile=shouldTransferSkinningProfile(policy,sourceProfile.method);
    std::vector<double> factors;std::size_t assigned=0;
    if(copyProfile)
    {
        validateSkinningProfile(sourceProfile,source.count());validateSkinningProfile(targetProfile,targetCount);
        if(targetProfile.method==SkinningMethod::Blended)factors=targetProfile.blendWeights;
        else factors.assign(targetCount,targetProfile.method==SkinningMethod::DualQuaternion?1.:0.);
    }
    for(const auto &point:binding.points)
    {
        std::map<BoneIndex,double> values;double profileMass=0,profileSum=0;
        for(unsigned c=0;c<3;++c)
        {
            const auto coefficient=std::max(0.,point.barycentric[c]);const auto id=binding.mode==SurfaceBindingMode::ControlPoints?point.vertices[c]:source.sourcePoint(point.vertices[c]);
            double mass=0;for(const auto weight:source.weights(id))if(weight.weight>0)
            {const auto contribution=coefficient*weight.weight;values[weight.bone]+=contribution;if(copyProfile)mass+=contribution;}
            if(copyProfile&&mass>0)
            {
                const auto amount=sourceProfile.method==SkinningMethod::Blended?sourceProfile.blendWeights.at(id):
                                  (sourceProfile.method==SkinningMethod::DualQuaternion?1.:0.);
                profileMass+=mass;profileSum+=mass*amount;
            }
        }
        std::vector<SkinWeight> weights;for(const auto [bone,value]:values)if(value>0)weights.push_back({bone,value});
        if(weights.empty()){if(fallback==NoBone)throw std::invalid_argument("Binding source has no usable skin");weights.push_back({fallback,1});}
        std::sort(weights.begin(),weights.end(),[](const auto &a,const auto &b){return a.weight!=b.weight?a.weight>b.weight:a.bone<b.bone;});if(maximumInfluences&&weights.size()>maximumInfluences)weights.resize(maximumInfluences);normalizeSkinWeights(weights,data.bones.size());
        if(copyProfile&&profileMass>0)
        {
            if(!std::isfinite(profileMass)||!std::isfinite(profileSum))throw std::overflow_error("Binding skinning profile overflow");
            factors[point.point]=std::clamp(profileSum/profileMass,0.,1.);++assigned;
        }
        if(target.topology)target.topology->points[point.point].weights=std::move(weights);else target.geometry.vertices[point.point].weights=std::move(weights);++result.transferred;
    }
    if(copyProfile&&assigned)
    {
        if(assigned==targetCount&&sourceProfile.method!=SkinningMethod::Blended)targetProfile={sourceProfile.method,{}};
        else if(sourceProfile.method==SkinningMethod::Blended||targetProfile.method==SkinningMethod::Blended||targetProfile.method!=sourceProfile.method)
            targetProfile={SkinningMethod::Blended,std::move(factors)};
    }
    if(target.topology){auto render=renderMeshTopology(*target.topology);target.geometry=std::move(render.geometry);target.textureCoordinates=std::move(render.textureCoordinates);target.triangleMaterials=std::move(render.triangleMaterials);}data=std::move(next);return result;
}
SkinTransferResult transferMeshBindingWeights(Object3DData &data,const SurfaceBinding &binding,std::size_t maximumInfluences,BoneIndex fallback,SkinningProfileTransfer policy)
{return transferWeights(data,binding,maximumInfluences,fallback,policy,nullptr);}
SkinTransferResult transferMeshBindingWeightsWithBasis(Object3DData &data,const SurfaceBinding &binding,
    const SurfaceBindingBasis &basis,std::size_t maximumInfluences,BoneIndex fallback,SkinningProfileTransfer policy)
{return transferWeights(data,binding,maximumInfluences,fallback,policy,&basis);}
} // namespace iiActorObject
