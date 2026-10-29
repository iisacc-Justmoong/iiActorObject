#pragma once
#include <iiActorObject/GlbAnimationPose.h>

namespace iiActorObject
{
enum class GlbStepConversion { Reject, Approximate };
struct GlbAnimationBakeOptions final
{
    double maximumSampleInterval = 1.0 / 60.0;
    double maximumPositionError = 0.0001;
    double maximumRotationError = 0.001;
    double maximumScaleError = 0.0001;
    double maximumMorphError = 0.0001;
    double maximumMatrixError = 0.00001;
    double staticDuration = 1.0;
    // Approximate STEP jumps over a reported interval no longer than this.
    double maximumStepTransition = 1.0 / 48000.0;
    GlbStepConversion steps = GlbStepConversion::Reject;
    // Require distinct float32 GLB times and distinct FBX ticks.
    bool requirePortableTimes = true;
    std::size_t maximumSampleTimes = 250'000;
    std::size_t maximumEvaluations = 1'000'000;
    std::size_t maximumOutputKeys = 16'000'000;
    std::size_t maximumCachedValues = 16'000'000;
    std::size_t maximumNodeVisits = 128'000'000;
    std::size_t maximumChannelValueVisits = 128'000'000;
    std::size_t maximumSourceKeyVisits = 16'000'000;
    std::size_t maximumSubdivisionDepth = 24;
};
struct GlbBakeErrors final
{
    // Position/scale use Euclidean distance; rotation is radians.
    double position = 0, rotation = 0, scale = 0, morph = 0;
};
struct GlbBakeStepTransition final { double from = 0, at = 0; };
struct GlbBakedAnimation final
{
    Object3DHandle object; // Borrowed; the engine remains the owner.
    AnimationClip clip;
    std::vector<AnimationMorphBinding> morphBindings;
    std::vector<GlbBakeStepTransition> stepTransitions;
    std::size_t sampleTimes = 0, evaluations = 0;
    // Finite quarter-point checks only, excluding reported STEP transitions.
    GlbBakeErrors maximumObservedError;
};
// Preserves per-mesh morph identity. STEP changes require an explicit temporal
// approximation; output reports its affected intervals. No input is mutated.
[[nodiscard]] IIACTOROBJECT_EXPORT GlbBakedAnimation bakeGlbAnimationClip(
    const Object3DHandle &object,std::span<const GlbAnimationNodeBinding> nodes,
    const GlbAnimationSource &source,const GlbAnimationBakeOptions &options = {});
[[nodiscard]] IIACTOROBJECT_EXPORT GlbAnimationPoseSample sampleGlbBakedAnimation(
    const GlbBakedAnimation &baked,double seconds,AnimationWrap wrap = AnimationWrap::Clamp);
} // namespace iiActorObject
