#pragma once
#include <iiActorObject/Animation.h>
#include <iiActorObject/Object3DInterchange.h>
#include <array>

namespace iiActorObject
{
enum class GlbAnimationPath { Translation, Rotation, Scale, Weights };
enum class GlbAnimationInterpolation { Linear, Step, CubicSpline };

// A channel keeps the original source node and unresampled glTF key data.
// Cubic data is grouped as in-tangent, value, out-tangent per key.
struct GlbAnimationChannel final
{
    std::size_t sourceNode = 0;
    GlbAnimationPath path = GlbAnimationPath::Translation;
    GlbAnimationInterpolation interpolation = GlbAnimationInterpolation::Linear;
    std::size_t components = 0;
    std::vector<double> seconds, values;
};
struct GlbAnimationValue final
{
    std::size_t sourceNode = 0;
    GlbAnimationPath path = GlbAnimationPath::Translation;
    std::vector<double> values;
};
class IIACTOROBJECT_EXPORT GlbAnimationSource final
{
  public:
    GlbAnimationSource(std::string name, std::vector<GlbAnimationChannel> channels);
    [[nodiscard]] std::string_view name() const noexcept;
    [[nodiscard]] double duration() const noexcept;
    [[nodiscard]] std::span<const GlbAnimationChannel> channels() const noexcept;
    // Returns source-node values, leaving the caller's scene and pose unchanged.
    [[nodiscard]] std::vector<GlbAnimationValue> sample(double seconds,
        AnimationWrap wrap = AnimationWrap::Clamp) const;
  private:
    std::string name_;
    std::vector<GlbAnimationChannel> channels_;
    double duration_ = 0;
};

struct GlbAnimationImportOptions final
{
    GlbImportOptions asset;
    std::size_t maximumAnimations = 1024;
    std::size_t maximumChannels = 100000;
    std::size_t maximumSamplers = 100000;
    // Sum of decoded input and output scalars, counting each channel use.
    std::size_t maximumDecodedValues = 16000000;
};
struct GlbAnimationNodeBinding final
{
    std::size_t sourceNode = 0, parentSourceNode = NoBone;
    BoneIndex bone = NoBone;
    std::size_t mesh = NoBone;
    // Selected-scene local matrix in glTF column-major order, before animation.
    std::array<double,16> localMatrix{};
    // Authored TRS is retained separately, including negative scale signs.
    Transform localTransform;
    bool hasMatrix = false, skinnedMesh = false;
};
struct GlbAnimatedImport final
{
    GlbImportResult asset;
    std::vector<GlbAnimationSource> animations;
    // Selected-scene order; every mesh and joint maps by identity, not name.
    std::vector<GlbAnimationNodeBinding> nodes;
};
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<GlbAnimationSource> readGlbAnimationSources(
    std::span<const std::uint8_t> bytes, const GlbAnimationImportOptions &options = {});
[[nodiscard]] IIACTOROBJECT_EXPORT GlbAnimatedImport importObject3DGlbWithAnimations(
    const Object3DReference &engine, std::span<const std::uint8_t> bytes,
    const GlbAnimationImportOptions &options = {});
} // namespace iiActorObject
