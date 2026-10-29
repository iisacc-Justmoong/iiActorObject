#pragma once
#include <iiActorObject/GlbAnimationSource.h>

namespace iiActorObject
{
struct GlbAnimatedMorphValue final
{
    std::size_t mesh = 0, target = 0;
    double value = 0;
};
struct GlbAnimationPoseSample final
{
    // This handle does not own the engine or the asset.
    Object3DHandle object;
    std::vector<Transform> boneLocals;
    std::vector<GlbAnimatedMorphValue> morphs;
};
// Reconstructs selected-scene joint locals and mesh-specific morph values from
// the source channels. Unsupported rigid-node motion or shear is rejected.
[[nodiscard]] IIACTOROBJECT_EXPORT GlbAnimationPoseSample sampleGlbAnimationPose(
    const Object3DHandle &object, std::span<const GlbAnimationNodeBinding> nodes,
    const GlbAnimationSource &source,double seconds,AnimationWrap wrap=AnimationWrap::Clamp,
    double maximumMatrixError=0.00001);
// Components are in imported asset mesh order and must refer to that exact asset.
// The complete sample and destinations are checked before any mutation. For a
// static asset, the instance's synthetic root transform is left unchanged.
IIACTOROBJECT_EXPORT void applyGlbAnimationPose(const GlbAnimationPoseSample &sample,
    Pose &pose,std::span<SkeletalMeshComponent *const> meshes);
} // namespace iiActorObject
