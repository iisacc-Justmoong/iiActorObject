#pragma once
#include <iiActorObject/Object3DRig.h>
#include <exception>

namespace iiActorObject
{
// Rotation relative to this joint's original bind rotation, in local space.
struct PoseCorrectiveCondition final
{
    std::string joint;
    Quaternion rotation;
    double radiusRadians=.5;
};
// A sample activates its named morph near all of its joint rotations.
// Repeated morph names take the maximum of their sample activations.
struct PoseCorrectiveSample final
{
    std::string morph;
    std::vector<PoseCorrectiveCondition> conditions;
    double maximumWeight=1;
};
struct Object3DPoseCorrectiveProgram final
{
    std::vector<PoseCorrectiveSample> samples;
};
IIACTOROBJECT_EXPORT void validateObject3DPoseCorrectiveProgram(const Object3DPoseCorrectiveProgram &program);
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<std::byte> exportObject3DPoseCorrectiveProgram(
    const Object3DPoseCorrectiveProgram &program,std::size_t maximumBytes=64*1024*1024);
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DPoseCorrectiveProgram importObject3DPoseCorrectiveProgram(
    std::span<const std::byte> bytes,std::size_t maximumBytes=64*1024*1024);
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DRigHandle createObject3DPoseCorrectiveRig(
    const Object3DHandle &object,const Object3DPoseCorrectiveProgram &program,std::span<const BoneIndex> jointMapping={});
// Evaluate without changing pose or morph weights. The pose must use the rig's skeleton.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DRigEvaluation evaluateObject3DPoseCorrectives(
    const Object3DRigHandle &rig,const Pose &pose);
// Updates only named corrective morphs, preserving the actor's existing joint pose.
IIACTOROBJECT_EXPORT void applyObject3DPoseCorrectives(const Object3DRigHandle &rig,iiCharacter &actor);

// Ticks after iiCharacter's animation on the actor's host thread. The rig handle
// is borrowed; its core engine continues to own the rig and its program.
class IIACTOROBJECT_EXPORT PoseCorrectiveComponent final : public ActorComponent
{
  public:
    explicit PoseCorrectiveComponent(Object3DRigHandle rig);
    [[nodiscard]] const Object3DRigHandle &rig() const noexcept;
    [[nodiscard]] std::exception_ptr error() const noexcept;
    void update();
  protected:
    void onBeginPlay() noexcept override;
    void onTick(double seconds) noexcept override;
  private:
    void updateNoexcept() noexcept;
    Object3DRigHandle rig_;
    std::exception_ptr error_;
};
[[nodiscard]] IIACTOROBJECT_EXPORT PoseCorrectiveComponent &attachObject3DPoseCorrectives(
    iiCharacter &actor,const Object3DRigHandle &rig);
} // namespace iiActorObject
