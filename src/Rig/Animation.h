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
// Indices refer to one clip and its asset's mesh and logical render-morph order.
struct AnimationMorphBinding final
{
    std::size_t curve = 0;
    std::size_t mesh = 0;
    std::size_t target = 0;
};

class IIACTOROBJECT_EXPORT AnimationClip
{
  public:
    AnimationClip(std::shared_ptr<const Skeleton> skeleton, double duration, std::vector<BoneTrack> tracks,
                  std::vector<AnimationCurve> curves = {});
    // A nonempty bind describes the required per-instance deformation context.
    AnimationClip(std::shared_ptr<const Skeleton> skeleton, double duration, std::vector<BoneTrack> tracks,
                  std::vector<AnimationCurve> curves, std::vector<Transform> requiredDeformationBind);
    AnimationClip(const AnimationClip &) = default;
    AnimationClip &operator=(const AnimationClip &) = delete;
    [[nodiscard]] const std::shared_ptr<const Skeleton> &skeleton() const noexcept;
    [[nodiscard]] double duration() const noexcept;
    [[nodiscard]] std::span<const BoneTrack> tracks() const noexcept;
    [[nodiscard]] std::span<const AnimationCurve> curves() const noexcept;
    [[nodiscard]] std::span<const Transform> requiredDeformationBind() const noexcept;
    void sample(double seconds, Pose &destination, AnimationWrap wrap = AnimationWrap::Clamp) const;
    [[nodiscard]] double sampleCurve(std::size_t curve, double seconds,
                                     AnimationWrap wrap = AnimationWrap::Clamp) const;

  private:
    std::shared_ptr<const Skeleton> skeleton_;
    double duration_;
    std::vector<BoneTrack> tracks_;
    std::vector<std::size_t> trackForBone_;
    std::vector<AnimationCurve> curves_;
    std::vector<Transform> requiredDeformationBind_;
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
    [[nodiscard]] AnimationWrap wrap() const noexcept;
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
