#pragma once
#include <iiActorObject/Object3DProgressiveMorphData.h>
#include <iiActorObject/Object3DRig.h>
#include <iiActorObject/Object3DAnimation.h>

namespace iiActorObject
{
// The core rig translates named controls into weights for existing linear
// morphs on all meshes. Evaluations outside the key range extrapolate and may
// produce negative or greater-than-one output weights; values are not clamped.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DRigHandle createObject3DProgressiveMorphRig(
    const Object3DHandle &object,const ProgressiveMorphProgram &program);
[[nodiscard]] IIACTOROBJECT_EXPORT ProgressiveMorphProgram inspectObject3DProgressiveMorphRig(
    const Object3DRigHandle &rig);
// Scalar curves in this reserved namespace bind to same-named progressive
// controls. The mapping survives the existing project animation format.
inline constexpr std::string_view Object3DProgressiveCurvePrefix="iiActorObject/progressive/";
[[nodiscard]] IIACTOROBJECT_EXPORT std::string object3DProgressiveControlCurveName(std::string_view control);
// Samples a clip and its progressive rig together. All binding and component
// validation completes before the destination is changed.
IIACTOROBJECT_EXPORT void applyObject3DProgressiveAnimationClip(
    const Object3DRigHandle &rig,const AnimationClip &clip,std::span<const AnimationMorphBinding> morphBindings,
    double seconds,Pose &destination,std::span<SkeletalMeshComponent *const> meshes,
    AnimationWrap wrap=AnimationWrap::Clamp);
} // namespace iiActorObject
