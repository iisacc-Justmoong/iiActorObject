#pragma once
#include <iiActorObject/PoseAiming.h>
#include <iiActorObject/PoseFrames.h>
#include <span>
#include <vector>

namespace iiActorObject
{
struct PoseReferenceOptions final
{
    std::size_t maximumConstraints=256;
    PoseAimOptions aiming;
    PoseFrameOptions frames;
    bool operator==(const PoseReferenceOptions &) const=default;
};
enum class PoseReferenceStepKind { Direction, Frame };
struct PoseReferenceStep final
{
    PoseReferenceStepKind kind=PoseReferenceStepKind::Direction;
    std::size_t index=0;
    bool operator==(const PoseReferenceStep &) const=default;
};
// Authoring metadata. Evaluated poses and caches belong to the core asset.
struct PoseReference final
{
    std::vector<PoseJointAim> directions;
    std::vector<PoseJointFrame> frames;
    PoseReferenceOptions options;
    // Empty preserves the legacy directions-then-frames order. Otherwise this
    // is a permutation containing each direction and frame exactly once.
    std::vector<PoseReferenceStep> order;
    bool operator==(const PoseReference &) const=default;
};
// Pure query: applies the specified order, or directions then frames when empty.
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<Transform> evaluatePoseReference(
    const Pose &source,const PoseReference &reference);
struct PoseReferenceDirectionQuery final
{
    BoneIndex joint=NoBone,descendant=NoBone;
};
struct PoseReferenceDirectionOptions final
{
    std::size_t maximumQueries=4096,maximumAncestorVisits=1'000'000;
};
// Measure nonzero object-space descendant displacements after evaluating the
// program on the skeleton's bind pose. Translation cancels in local hierarchy
// space, including for very large object origins.
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<Vector3> measureReferencePoseDirections(
    const std::shared_ptr<const Skeleton> &skeleton,const PoseReference &reference,
    std::span<const PoseReferenceDirectionQuery> queries,const PoseReferenceDirectionOptions &options={});
}
