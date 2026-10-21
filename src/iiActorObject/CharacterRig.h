#pragma once
#include <iiActorObject/Skeleton.h>

namespace iiActorObject
{
struct JointControl
{
    BoneIndex bone = NoBone;
    Transform local;
    double weight = 1;
};
struct TwoBoneGoal
{
    BoneIndex root = NoBone, middle = NoBone, end = NoBone;
    Vector3 target, pole;
    double weight = 1;
};
struct TwoBoneResult
{
    Vector3 reached;
    bool clamped = false;
};
class IIACTOROBJECT_EXPORT CharacterRig
{
  public:
    static void apply(Pose &pose, std::span<const JointControl> controls);
    // Rigid, unit-scale chain; targets and pole are in skeleton/object space.
    [[nodiscard]] static TwoBoneResult solveTwoBone(Pose &pose, const TwoBoneGoal &goal);
};

struct FacialJointDriver
{
    BoneIndex bone = NoBone;
    std::size_t control = 0;
    Vector3 translation;
    Vector3 rotationAxis{1, 0, 0};
    double radians = 0;
    Vector3 scaleDelta;
};
inline constexpr std::size_t NoFacialControl = std::numeric_limits<std::size_t>::max();
struct FacialTerm
{
    std::size_t control = 0;
    std::size_t multiplyControl = NoFacialControl;
    double gain = 1, bias = 0;
};
struct FacialChannel
{
    std::string name;
    std::vector<FacialTerm> terms;
};

// Native semantic channels -> joint transforms, blendshapes and material values.
// This is an independent rig definition, not an Epic DNA reader or RigLogic ABI.
class IIACTOROBJECT_EXPORT FacialRig
{
  public:
    FacialRig(std::shared_ptr<const Skeleton> skeleton, std::vector<std::string> controls,
              std::vector<FacialJointDriver> joints, std::vector<FacialChannel> blendShapes,
              std::vector<FacialChannel> animatedMaps = {});
    FacialRig(const FacialRig &) = default;
    FacialRig &operator=(const FacialRig &) = delete;
    [[nodiscard]] std::span<const std::string> controls() const noexcept;
    [[nodiscard]] std::span<const FacialChannel> blendShapes() const noexcept;
    [[nodiscard]] std::span<const FacialChannel> animatedMaps() const noexcept;
    // Values are in [0,1]; input/output spans must not overlap.
    // Driven joints start at bind pose on every evaluation.
    void evaluate(Pose &pose, std::span<const double> values, std::span<double> blendShapeValues,
                  std::span<double> animatedMapValues) const;

  private:
    std::shared_ptr<const Skeleton> skeleton_;
    std::vector<std::string> controls_;
    std::vector<FacialJointDriver> joints_;
    std::vector<BoneIndex> drivenBones_;
    std::vector<std::vector<std::size_t>> jointDrivers_;
    std::vector<FacialChannel> shapes_, maps_;
};
} // namespace iiActorObject
