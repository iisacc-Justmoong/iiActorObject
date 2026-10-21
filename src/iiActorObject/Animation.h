#pragma once
#include <iiActorObject/Skeleton.h>

namespace iiActorObject
{
enum class AnimationWrap
{
    Clamp,
    Loop
};
struct TransformKey
{
    double seconds = 0;
    Transform value;
};
struct BoneTrack
{
    BoneIndex bone = NoBone;
    std::vector<TransformKey> keys;
};
struct ScalarKey
{
    double seconds = 0;
    double value = 0;
};
struct AnimationCurve
{
    std::string name;
    std::vector<ScalarKey> keys;
};

class IIACTOROBJECT_EXPORT AnimationClip
{
  public:
    AnimationClip(std::shared_ptr<const Skeleton> skeleton, double duration, std::vector<BoneTrack> tracks,
                  std::vector<AnimationCurve> curves = {});
    AnimationClip(const AnimationClip &) = default;
    AnimationClip &operator=(const AnimationClip &) = delete;
    [[nodiscard]] const std::shared_ptr<const Skeleton> &skeleton() const noexcept;
    [[nodiscard]] double duration() const noexcept;
    [[nodiscard]] std::span<const AnimationCurve> curves() const noexcept;
    void sample(double seconds, Pose &destination, AnimationWrap wrap = AnimationWrap::Clamp) const;
    [[nodiscard]] double sampleCurve(std::size_t curve, double seconds,
                                     AnimationWrap wrap = AnimationWrap::Clamp) const;

  private:
    std::shared_ptr<const Skeleton> skeleton_;
    double duration_;
    std::vector<BoneTrack> tracks_;
    std::vector<std::size_t> trackForBone_;
    std::vector<AnimationCurve> curves_;
};

class IIACTOROBJECT_EXPORT AnimationPlayer
{
  public:
    explicit AnimationPlayer(std::shared_ptr<Pose> destination);
    void play(std::shared_ptr<const AnimationClip> clip, AnimationWrap wrap = AnimationWrap::Loop);
    void pause() noexcept;
    void resume() noexcept;
    void stop();
    void seek(double seconds);
    bool advance(double deltaSeconds);
    void setRate(double rate);
    [[nodiscard]] double rate() const noexcept;
    [[nodiscard]] double time() const noexcept;
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] const std::shared_ptr<const AnimationClip> &clip() const noexcept;

  private:
    std::shared_ptr<Pose> destination_;
    std::shared_ptr<const AnimationClip> clip_;
    AnimationWrap wrap_ = AnimationWrap::Loop;
    double time_ = 0, rate_ = 1;
    bool playing_ = false;
};
} // namespace iiActorObject
