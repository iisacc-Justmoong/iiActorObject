#include "Object3DProgressiveMorph.h"
#include "Object3DProgressiveMorphBackend.h"
#include "Object3DAnimationApply.h"
#include "Object3DNames.h"
#include <unordered_map>

namespace iiActorObject
{
std::string object3DProgressiveControlCurveName(std::string_view control)
{
    object3d_detail::validateName(control);
    return std::string(Object3DProgressiveCurvePrefix)+std::string(control);
}

void applyObject3DProgressiveAnimationClip(const Object3DRigHandle &handle,const AnimationClip &clip,
    std::span<const AnimationMorphBinding> bindings,double seconds,Pose &destination,
    std::span<SkeletalMeshComponent *const> meshes,AnimationWrap wrap)
{
    if(wrap!=AnimationWrap::Clamp&&wrap!=AnimationWrap::Loop)
        throw std::invalid_argument("Progressive animation: invalid wrap mode");
    auto &rig=handle.get();
    if(!object3d_detail::isProgressiveMorphRigSession(rig))
        throw std::invalid_argument("Progressive animation: rig backend mismatch");
    const auto &object=rig.object();const auto &asset=object.get();
    if(clip.skeleton().get()!=asset.skeleton().get()||destination.skeleton().get()!=asset.skeleton().get()||
       (!asset.hasRig()&&!clip.tracks().empty()))
        throw std::invalid_argument("Progressive animation: clip or pose skeleton mismatch");
    constexpr std::size_t limit=128'000'000;
    if(clip.curves().size()>limit||bindings.size()>limit)
        throw std::length_error("Progressive animation: curve binding budget");
    const auto &definition=rig.definition();
    std::unordered_map<std::string_view,std::size_t> controlIndices;
    for(std::size_t i=0;i<definition.rawControls.size();++i)controlIndices.emplace(definition.rawControls[i],i);
    auto controls=rig.resolveNamed({},RigControlSpace::Raw);
    for(std::size_t i=0;i<clip.curves().size();++i)
    {
        const std::string_view name=clip.curves()[i].name;
        if(!name.starts_with(Object3DProgressiveCurvePrefix))continue;
        const auto found=controlIndices.find(name.substr(Object3DProgressiveCurvePrefix.size()));
        if(found==controlIndices.end())throw std::invalid_argument("Progressive animation: unknown reserved control curve");
        controls[found->second]=clip.sampleCurve(i,seconds,wrap);
    }
    const auto evaluated=rig.evaluate(controls);
    Pose sampled(asset.skeleton());
    if(!destination.deformationBindTransforms().empty())sampled.setDeformationBind(destination.deformationBindTransforms());
    clip.sample(seconds,sampled,wrap);
    struct MorphSample final {std::size_t mesh,target;double value;};
    std::vector<MorphSample> values;values.reserve(bindings.size());
    for(const auto &binding:bindings)
    {
        if(binding.curve>=clip.curves().size())throw std::out_of_range("Progressive animation: curve index");
        values.push_back({binding.mesh,binding.target,clip.sampleCurve(binding.curve,seconds,wrap)});
    }
    std::unordered_map<std::string_view,std::size_t> targets;
    for(std::size_t i=0;i<definition.blendShapes.size();++i)targets.emplace(definition.blendShapes[i],i);
    std::size_t visits=0;
    for(std::size_t mesh=0;mesh<asset.data().meshes.size();++mesh)
    {
        const auto &morphs=asset.data().meshes[mesh].geometry.morphTargets;
        if(morphs.size()>limit-visits)throw std::length_error("Progressive animation: target visit budget");
        visits+=morphs.size();
        for(std::size_t target=0;target<morphs.size();++target)
            if(const auto found=targets.find(morphs[target].name);found!=targets.end())
            {
                if(values.size()>=limit)throw std::length_error("Progressive animation: target sample budget");
                values.push_back({mesh,target,evaluated.blendShapes[found->second]});
            }
    }
    object3d_detail::applyAnimationPose(object,sampled.locals(),values,destination,meshes,false,
                                         "Progressive animation",asset.hasRig());
}
} // namespace iiActorObject
