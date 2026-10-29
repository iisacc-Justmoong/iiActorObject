#include "AnimationRetarget.h"
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace iiActorObject
{
AnimationClip retargetAnimation(const AnimationClip &source, const Pose &sourceReference,
    const Pose &targetReference, std::span<const PoseRetargetJoint> joints,
    std::span<const double> sampleTimes, const AnimationRetargetOptions &options)
{
    if (source.skeleton().get() != sourceReference.skeleton().get())
        throw std::invalid_argument("Animation retarget source skeleton mismatch");
    if (options.maximumSamples < 2 || options.maximumSamples > 1'000'000 ||
        options.maximumOutputKeys == 0 || options.maximumOutputKeys > 10'000'000)
        throw std::invalid_argument("Invalid animation retarget budget");
    if (sampleTimes.size() < 2 || sampleTimes.size() > options.maximumSamples)
        throw std::length_error("Animation retarget sample count");
    if (sampleTimes.front() != 0 || sampleTimes.back() != source.duration())
        throw std::invalid_argument("Animation retarget samples must include both endpoints");
    double previous = -1;
    for (const double time : sampleTimes)
    {
        if (!std::isfinite(time) || time < 0 || time > source.duration() || time <= previous)
            throw std::invalid_argument("Animation retarget sample times must be finite and strictly increasing");
        previous = time;
    }

    const auto targetCount = targetReference.skeleton()->bones().size();
    if (targetCount == 0 || sampleTimes.size() > options.maximumOutputKeys / targetCount)
        throw std::length_error("Animation retarget output key budget");
    auto outputKeys = targetCount * sampleTimes.size();
    if (options.copyScalarCurves)
        for (const auto &curve : source.curves())
        {
            if (curve.keys.size() > options.maximumOutputKeys - outputKeys)
                throw std::length_error("Animation retarget scalar key budget");
            outputKeys += curve.keys.size();
        }

    // Check mapping and both references before allocating the output tracks.
    (void)retargetPose(sourceReference, sourceReference, targetReference, joints, options.pose);
    Pose sourcePose(source.skeleton());
    if(!sourceReference.deformationBindTransforms().empty())
        (void)sourcePose.setDeformationBind(sourceReference.deformationBindTransforms());
    // Bound source clips must match the chosen reference before output allocation.
    source.sample(sampleTimes.front(),sourcePose,AnimationWrap::Clamp);
    std::vector<BoneTrack> tracks(targetCount);
    for (BoneIndex bone = 0; bone < targetCount; ++bone)
    {
        tracks[bone].bone = bone;
        tracks[bone].keys.reserve(sampleTimes.size());
    }
    for (const double time : sampleTimes)
    {
        source.sample(time, sourcePose, AnimationWrap::Clamp);
        const auto locals = retargetPose(sourceReference, sourcePose, targetReference, joints, options.pose);
        for (BoneIndex bone = 0; bone < targetCount; ++bone)
            tracks[bone].keys.push_back({time, locals[bone]});
    }
    std::vector<AnimationCurve> curves;
    if (options.copyScalarCurves)
        curves.assign(source.curves().begin(), source.curves().end());
    const auto bind=targetReference.deformationBindTransforms();
    return AnimationClip(targetReference.skeleton(), source.duration(), std::move(tracks), std::move(curves),
                         std::vector<Transform>(bind.begin(),bind.end()));
}
}
