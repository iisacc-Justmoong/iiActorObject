#pragma once
#include "Object3DPoseCorrectives.h"

namespace iiActorObject::object3d_detail
{
struct PoseCorrectiveBoundCondition final {std::size_t joint=0;Quaternion rotation;double radius=0;};
struct PoseCorrectiveBoundSample final
{
    std::size_t morph=0;
    std::vector<PoseCorrectiveBoundCondition> conditions;
    double maximumWeight=0;
};
[[nodiscard]] bool isPoseCorrectiveDriver(const Object3DRigDriver &driver) noexcept;
[[nodiscard]] std::size_t poseCorrectiveRigWork(const Object3DRigSession &rig);
// Borrowed compiled conditions in the core rig's own joint and morph order.
[[nodiscard]] std::span<const PoseCorrectiveBoundSample> poseCorrectiveRigBoundSamples(const Object3DRigSession &rig);
[[nodiscard]] std::unique_ptr<Object3DRigDriver> makePoseCorrectiveDriver(Object3DPoseCorrectiveProgram program);
}
