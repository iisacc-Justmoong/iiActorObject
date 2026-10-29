#pragma once
#include <iiActorObject/Object3DEngine.h>
#include <iiActorObject/PoseAiming.h>
#include <iiActorObject/PoseFrames.h>
#include <cstddef>
#include <span>

namespace iiActorObject
{
struct Object3DPoseSnapshotOptions final
{
    std::size_t maximumPoints=4'000'000;
    std::size_t maximumLayouts=8'000'000;
    std::size_t maximumDeltas=16'000'000;
    std::size_t maximumInfluenceVisits=128'000'000;
    std::size_t maximumAddedNormals=8'000'000;
    std::size_t maximumJointAims=256;
};
// Linearize only after baking the selected pose and morph baseline. Subsequent
// animation may differ from the original nonlinear skinning.
enum class Object3DSnapshotSkinning { Preserve, LinearizeAtBind };
// Exchange snapshot only: preserve the source evaluated at these locals, including
// morph interpolation. This does not preserve arbitrary subsequent animations.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DData bakeObject3DPoseSnapshot(const Object3DAsset &source,
    std::span<const Transform> locals,const Object3DPoseSnapshotOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DData bakeObject3DPoseSnapshot(const Object3DAsset &source,
    std::span<const Transform> locals,const Object3DPoseSnapshotOptions &options,Object3DSnapshotSkinning skinning);
// New assets are registered with the same core engine; failure leaves it unchanged.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DHandle createObject3DPoseSnapshot(const Object3DHandle &source,
    std::span<const Transform> locals,const Object3DPoseSnapshotOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DHandle createObject3DPoseSnapshot(const Object3DHandle &source,
    std::span<const Transform> locals,const Object3DPoseSnapshotOptions &options,Object3DSnapshotSkinning skinning);
// Bake the full evaluated deformation. One weight buffer per source mesh, in
// its runtime morph-channel order. Retained channels are additive offsets from
// this baseline: output(w) == source(baseline + w) at the supplied pose.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DData bakeObject3DDeformationSnapshot(const Object3DAsset &source,
    std::span<const Transform> locals,std::span<const std::vector<double>> morphWeights,
    const Object3DPoseSnapshotOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DData bakeObject3DDeformationSnapshot(const Object3DAsset &source,
    std::span<const Transform> locals,std::span<const std::vector<double>> morphWeights,
    const Object3DPoseSnapshotOptions &options,Object3DSnapshotSkinning skinning);
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DHandle createObject3DDeformationSnapshot(const Object3DHandle &source,
    std::span<const Transform> locals,std::span<const std::vector<double>> morphWeights,
    const Object3DPoseSnapshotOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DHandle createObject3DDeformationSnapshot(const Object3DHandle &source,
    std::span<const Transform> locals,std::span<const std::vector<double>> morphWeights,
    const Object3DPoseSnapshotOptions &options,Object3DSnapshotSkinning skinning);
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DHandle createReferenceObject3DPoseSnapshot(const Object3DHandle &source,
    const Object3DPoseSnapshotOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DHandle createAimedObject3DPoseSnapshot(const Object3DHandle &source,
    std::span<const PoseJointAim> aims,const Object3DPoseSnapshotOptions &options={},const PoseAimOptions &aimOptions={});
// Apply all directions, then ordered frames in one core-owned temporary pose.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DHandle createOrientedObject3DPoseSnapshot(const Object3DHandle &source,
    std::span<const PoseJointAim> aims,std::span<const PoseJointFrame> frames,
    const Object3DPoseSnapshotOptions &options={},const PoseAimOptions &aimOptions={},const PoseFrameOptions &frameOptions={});
}
