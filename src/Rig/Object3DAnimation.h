#pragma once
#include <iiActorObject/Animation.h>
#include <iiActorObject/Object3DEngine.h>

namespace iiActorObject
{
// Applies a clip and explicitly mapped morph curves to an instance of this exact
// asset. Components must be in asset mesh order and bound to destination.
// All inputs are validated before the destination pose or morphs change. A
// morph-only static asset retains its instance's synthetic root transform.
IIACTOROBJECT_EXPORT void applyObject3DAnimationClip(
    const Object3DHandle &object, const AnimationClip &clip,
    std::span<const AnimationMorphBinding> morphBindings, double seconds,
    Pose &destination, std::span<SkeletalMeshComponent *const> meshes,
    AnimationWrap wrap = AnimationWrap::Clamp);
} // namespace iiActorObject
