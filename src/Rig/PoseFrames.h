#pragma once
#include <iiActorObject/Skeleton.h>
#include <cstddef>
namespace iiActorObject
{
struct PoseFrameOptions final
{
    std::size_t maximumAncestorVisits=1'000'000;
    double minimumPlaneSine=1e-8,orthogonalityTolerance=1e-10;
    bool operator==(const PoseFrameOptions &) const=default;
};
struct PoseJointFrame final
{
    BoneIndex joint=NoBone,primaryDescendant=NoBone,planeDescendant=NoBone;
    Vector3 objectDirection,objectPlaneNormal;
    bool operator==(const PoseJointFrame &) const=default;
};
[[nodiscard]] IIACTOROBJECT_EXPORT Transform orientPoseJointFrame(const Pose &pose,const PoseJointFrame &frame,const PoseFrameOptions &options={});
}
