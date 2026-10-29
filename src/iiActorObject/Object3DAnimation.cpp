#include "Object3DAnimation.h"
#include "Object3DAnimationApply.h"

namespace iiActorObject
{
namespace
{
struct MorphSample final { std::size_t mesh,target;double value; };
}
void applyObject3DAnimationClip(const Object3DHandle &object,const AnimationClip &clip,
    std::span<const AnimationMorphBinding> bindings,double seconds,Pose &destination,
    std::span<SkeletalMeshComponent *const> meshes,AnimationWrap wrap)
{
    if(wrap!=AnimationWrap::Clamp&&wrap!=AnimationWrap::Loop)
        throw std::invalid_argument("Object 3D animation: invalid wrap mode");
    const auto &asset=object.get();
    if(clip.skeleton().get()!=asset.skeleton().get()||destination.skeleton().get()!=asset.skeleton().get()||
       (!asset.hasRig()&&!clip.tracks().empty()))
        throw std::invalid_argument("Object 3D animation: clip or pose skeleton mismatch");
    Pose sampled(asset.skeleton());
    if(!destination.deformationBindTransforms().empty())sampled.setDeformationBind(destination.deformationBindTransforms());
    clip.sample(seconds,sampled,wrap);
    std::vector<MorphSample> values;values.reserve(bindings.size());
    for(const auto &binding:bindings)
    {
        if(binding.curve>=clip.curves().size())throw std::out_of_range("Object 3D animation: curve index");
        values.push_back({binding.mesh,binding.target,clip.sampleCurve(binding.curve,seconds,wrap)});
    }
    object3d_detail::applyAnimationPose(object,sampled.locals(),values,destination,meshes,false,
                                       "Object 3D animation",asset.hasRig());
}
} // namespace iiActorObject
