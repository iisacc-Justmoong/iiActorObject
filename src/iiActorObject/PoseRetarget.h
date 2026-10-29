#pragma once
#include <iiActorObject/Skeleton.h>
#include <span>
#include <vector>

namespace iiActorObject
{
struct PoseRetargetJoint final
{
    BoneIndex source=NoBone,target=NoBone;
};
struct PoseRetargetOptions final
{
    Quaternion sourceToTargetBasis;
    double translationScale=1.0;
    bool transferRootTranslation=false;
    std::size_t maximumJoints=65536;
};
// Rotation-only retargeting between two reference poses. The result uses the
// target skeleton's stable IDs and retains target reference scales and non-root
// positions. Root movement is opt-in. Input poses are not changed. Positive
// uniform local scales and rotation-representable deformation binds are required.
// Apply the output to a pose with the target reference's deformation bind.
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<Transform> retargetPose(
    const Pose &sourceReference,const Pose &sourcePose,const Pose &targetReference,
    std::span<const PoseRetargetJoint> joints,const PoseRetargetOptions &options={});
}
