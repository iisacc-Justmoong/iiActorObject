#include "CharacterMath.h"
#include <iiActorObject/Animation.h>
#include <unordered_set>

namespace iiActorObject
{
using namespace character_detail;
namespace
{
template <class Keys> void validateKeys(const Keys &keys, double duration)
{
    if (keys.empty())
        throw std::invalid_argument("Animation track needs keyframes");
    double previous = -1;
    for (const auto &key : keys)
    {
        if (!std::isfinite(key.seconds) || key.seconds < 0 || key.seconds > duration ||
            key.seconds <= previous)
            throw std::invalid_argument("Keys must have strictly increasing times within duration");
        previous = key.seconds;
    }
}
template <class Keys, class Interpolate>
auto sampleKeys(const Keys &keys, double time, Interpolate interpolate)
{
    if (time <= keys.front().seconds)
        return keys.front().value;
    if (time >= keys.back().seconds)
        return keys.back().value;
    const auto next = std::upper_bound(keys.begin(), keys.end(), time,
                                       [](double t, const auto &key) { return t < key.seconds; });
    const auto &previous = *(next - 1);
    return interpolate(previous.value, next->value,
                       (time - previous.seconds) / (next->seconds - previous.seconds));
}
} // namespace
AnimationClip::AnimationClip(std::shared_ptr<const Skeleton> skeleton, double duration,
                             std::vector<BoneTrack> tracks, std::vector<AnimationCurve> curves)
    : skeleton_(std::move(skeleton)), duration_(duration), tracks_(std::move(tracks)),
      curves_(std::move(curves))
{
    if (!skeleton_ || !std::isfinite(duration_) || duration_ <= 0)
        throw std::invalid_argument("Animation requires skeleton and positive finite duration");
    trackForBone_.assign(skeleton_->bones().size(), NoBone);
    for (std::size_t i = 0; i < tracks_.size(); ++i)
    {
        auto &track = tracks_[i];
        if (track.bone >= trackForBone_.size() || trackForBone_[track.bone] != NoBone)
            throw std::invalid_argument("Animation bone tracks must be unique and valid");
        trackForBone_[track.bone] = i;
        validateKeys(track.keys, duration_);
        for (auto &key : track.keys)
            key.value = skeletalTransform(key.value);
        for (std::size_t k = 1; k < track.keys.size(); ++k)
        {
            const auto a = track.keys[k - 1].value.scale, b = track.keys[k].value.scale;
            if (std::signbit(a.x) != std::signbit(b.x) || std::signbit(a.y) != std::signbit(b.y) ||
                std::signbit(a.z) != std::signbit(b.z))
                throw std::invalid_argument("Animation scale interpolation must not cross zero");
        }
    }
    std::unordered_set<std::string> names;
    for (const auto &curve : curves_)
    {
        if (curve.name.empty() || !names.insert(curve.name).second)
            throw std::invalid_argument("Curve names must be unique");
        validateKeys(curve.keys, duration_);
        for (const auto &key : curve.keys)
            if (!std::isfinite(key.value))
                throw std::invalid_argument("Curve values must be finite");
    }
}
const std::shared_ptr<const Skeleton> &AnimationClip::skeleton() const noexcept
{
    return skeleton_;
}
double AnimationClip::duration() const noexcept
{
    return duration_;
}
std::span<const AnimationCurve> AnimationClip::curves() const noexcept
{
    return curves_;
}
void AnimationClip::sample(double seconds, Pose &destination, AnimationWrap wrap) const
{
    if (destination.skeleton().get() != skeleton_.get())
        throw std::invalid_argument("Animation skeleton identity mismatch");
    const double t = sampleTime(seconds, duration_, wrap == AnimationWrap::Loop);
    for (BoneIndex i = 0; i < trackForBone_.size(); ++i)
    {
        const auto track = trackForBone_[i];
        const auto value =
            track == NoBone ? skeleton_->bones()[i].bindTransform : sampleKeys(tracks_[track].keys, t, blend);
        destination.setLocal(i, value);
    }
}
double AnimationClip::sampleCurve(std::size_t curve, double seconds, AnimationWrap wrap) const
{
    if (curve >= curves_.size())
        throw std::out_of_range("Animation curve index");
    return sampleKeys(curves_[curve].keys, sampleTime(seconds, duration_, wrap == AnimationWrap::Loop),
                      [](double a, double b, double t) { return std::lerp(a, b, t); });
}
AnimationPlayer::AnimationPlayer(std::shared_ptr<Pose> destination) : destination_(std::move(destination))
{
    if (!destination_)
        throw std::invalid_argument("Animation player requires a pose");
}
void AnimationPlayer::play(std::shared_ptr<const AnimationClip> clip, AnimationWrap wrap)
{
    if (!clip || clip->skeleton().get() != destination_->skeleton().get())
        throw std::invalid_argument("Animation clip skeleton mismatch");
    clip->sample(0, *destination_);
    clip_ = std::move(clip);
    wrap_ = wrap;
    time_ = 0;
    playing_ = true;
}
void AnimationPlayer::pause() noexcept
{
    playing_ = false;
}
void AnimationPlayer::resume() noexcept
{
    playing_ = static_cast<bool>(clip_);
}
void AnimationPlayer::stop()
{
    destination_->reset();
    clip_.reset();
    playing_ = false;
    time_ = 0;
}
void AnimationPlayer::setRate(double rate)
{
    if (!std::isfinite(rate))
        throw std::invalid_argument("Playback rate must be finite");
    rate_ = rate;
}
double AnimationPlayer::rate() const noexcept
{
    return rate_;
}
double AnimationPlayer::time() const noexcept
{
    return time_;
}
bool AnimationPlayer::isPlaying() const noexcept
{
    return playing_;
}
const std::shared_ptr<const AnimationClip> &AnimationPlayer::clip() const noexcept
{
    return clip_;
}
void AnimationPlayer::seek(double seconds)
{
    if (!std::isfinite(seconds))
        throw std::invalid_argument("Animation time must be finite");
    if (!clip_)
        throw std::logic_error("No animation clip");
    const auto next = sampleTime(seconds, clip_->duration(), wrap_ == AnimationWrap::Loop);
    clip_->sample(next, *destination_);
    time_ = next;
}
bool AnimationPlayer::advance(double delta)
{
    if (!std::isfinite(delta) || delta < 0)
        throw std::invalid_argument("Animation delta must be finite and nonnegative");
    if (!playing_ || delta == 0 || rate_ == 0)
        return false;
    const long double candidate = static_cast<long double>(time_) + static_cast<long double>(delta) * rate_;
    if (!std::isfinite(candidate))
        throw std::overflow_error("Animation clock overflow");
    const long double duration = clip_->duration();
    long double wrapped =
        wrap_ == AnimationWrap::Loop ? std::fmod(candidate, duration) : std::clamp(candidate, 0.0L, duration);
    if (wrapped < 0)
        wrapped += duration;
    const double next = static_cast<double>(wrapped);
    clip_->sample(next, *destination_);
    const bool changed = next != time_;
    time_ = next;
    if (wrap_ == AnimationWrap::Clamp &&
        ((rate_ > 0 && candidate >= duration) || (rate_ < 0 && candidate <= 0)))
        playing_ = false;
    return changed;
}
} // namespace iiActorObject
