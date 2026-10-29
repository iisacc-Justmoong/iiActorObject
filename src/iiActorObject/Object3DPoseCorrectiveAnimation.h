#pragma once
#include <iiActorObject/Animation.h>
#include <iiActorObject/Object3DPoseCorrectives.h>
#include <optional>

namespace iiActorObject
{
enum class PoseCorrectiveCurveCollision { Reject, Replace };
// Conservative bounds the driver's possible outputs along each joint arc.
// ProbeOnly is faster but can miss a narrow correction between probes.
enum class PoseCorrectiveSampling { Conservative, ProbeOnly };
struct PoseCorrectiveAnimationOptions final
{
    double maximumSampleInterval = 1.0 / 60.0;
    double maximumWeightError = 0.001;
    std::size_t maximumSampleTimes = 250'000;
    std::size_t maximumEvaluations = 1'000'000;
    std::size_t maximumEvaluatedWeights = 16'000'000;
    std::size_t maximumStoredJointRotations = 4'000'000; // Conservative cache entries.
    std::size_t maximumBoneVisits = 128'000'000;
    std::size_t maximumRigOperations = 128'000'000;
    std::size_t maximumSourceKeyVisits = 16'000'000;
    std::size_t maximumSubdivisionDepth = 24;
    PoseCorrectiveCurveCollision collision = PoseCorrectiveCurveCollision::Reject;
    PoseCorrectiveSampling sampling = PoseCorrectiveSampling::Conservative;
};
struct PoseCorrectiveAnimationResult final
{
    AnimationClip clip;
    std::size_t sampleTimes;
    std::size_t evaluations;
    double maximumObservedWeightError;
    // Absent in ProbeOnly mode. Includes numeric safety margins; this is not an
    // interval-arithmetic proof or a bound on errors introduced by exporters.
    std::optional<double> maximumConservativeWeightError;
};
// Samples a core-owned corrective rig without changing the input clip or actor.
// Conservative sampling also bounds possible weights between sampled positions.
[[nodiscard]] IIACTOROBJECT_EXPORT PoseCorrectiveAnimationResult bakeObject3DPoseCorrectiveAnimation(
    const Object3DRigHandle &rig, const AnimationClip &clip,
    const PoseCorrectiveAnimationOptions &options = {});
} // namespace iiActorObject
