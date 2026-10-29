#pragma once
#include <iiActorObject/Skeleton.h>
#include <cstddef>
#include <vector>
namespace iiActorObject
{
struct PoseAimOptions final {std::size_t maximumAncestorVisits=1'000'000;bool operator==(const PoseAimOptions &) const=default;};
struct PoseJointAim final
{
    BoneIndex joint=NoBone,descendant=NoBone;
    Vector3 objectDirection;
    // Additional descendant anchors contribute equally with descendant.
    // Their centroid is aimed without replacing any original bone.
    std::vector<BoneIndex> additionalDescendants;
    bool operator==(const PoseJointAim &) const=default;
};
// Pure query. Preserve helper joints, local position/scale and the input Pose revision.
[[nodiscard]] IIACTOROBJECT_EXPORT Transform aimPoseJoint(const Pose &pose,BoneIndex joint,BoneIndex descendant,
    Vector3 objectDirection,const PoseAimOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT Transform aimPoseJoint(const Pose &pose,const PoseJointAim &aim,
    const PoseAimOptions &options={});
}
