#pragma once
#include <iiActorObject/Animation.h>
#include <iiActorObject/PoseRetarget.h>

namespace iiActorObject
{
struct AnimationRetargetOptions final
{
    PoseRetargetOptions pose;
    std::size_t maximumSamples = 100000;
    std::size_t maximumOutputKeys = 1000000;
    bool copyScalarCurves = false;
};

// Sample an existing clip at explicit times and bake the pose retarget into an
// independent target-skeleton clip. Scalar curves are copied only on request.
// A target reference deformation bind becomes a requirement of the clip.
[[nodiscard]] IIACTOROBJECT_EXPORT AnimationClip retargetAnimation(
    const AnimationClip &source, const Pose &sourceReference, const Pose &targetReference,
    std::span<const PoseRetargetJoint> joints, std::span<const double> sampleTimes,
    const AnimationRetargetOptions &options = {});
}
