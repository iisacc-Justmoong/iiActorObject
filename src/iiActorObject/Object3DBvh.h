#pragma once
#include <iiActorObject/Skeleton.h>
#include <optional>
#include <string_view>

namespace iiActorObject
{
enum class BvhChannel { Xposition,Yposition,Zposition,Xrotation,Yrotation,Zrotation };
struct BvhJoint final
{
    std::string name;
    BoneIndex parent=NoBone;
    Vector3 offset;
    std::optional<Vector3> endSite;
    std::vector<BvhChannel> channels;
    std::size_t firstChannel=0;
};
struct BvhImportOptions final
{
    double scale=1;
    Quaternion basis;
    std::size_t maximumBytes=64*1024*1024;
    std::size_t maximumJoints=4096,maximumFrames=100000,maximumSamples=16'000'000;
};
class IIACTOROBJECT_EXPORT BvhMotion final
{
  public:
    [[nodiscard]] std::span<const BvhJoint> joints() const noexcept;
    [[nodiscard]] std::size_t frameCount() const noexcept;
    [[nodiscard]] double frameTime() const noexcept;
    [[nodiscard]] std::vector<Transform> frame(std::size_t index) const;
    [[nodiscard]] std::vector<Quaternion> worldRotations(std::size_t index) const;
  private:
    friend BvhMotion parseObject3DBvh(std::string_view,const BvhImportOptions &);
    std::vector<BvhJoint> joints_;
    std::vector<double> samples_;
    std::size_t channels_=0,frames_=0;
    double interval_=0,scale_=1;
    Quaternion basis_;
};
[[nodiscard]] IIACTOROBJECT_EXPORT BvhMotion parseObject3DBvh(std::string_view text,const BvhImportOptions &options={});
} // namespace iiActorObject
