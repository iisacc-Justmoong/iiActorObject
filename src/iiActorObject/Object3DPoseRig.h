#pragma once
#include <iiActorObject/Object3DBvh.h>
#include <iiActorObject/Object3DRig.h>

namespace iiActorObject
{
struct PoseUnitFrame final {std::string name;std::size_t frame=0;};
struct PoseRigUnit final
{
    std::string name;
    std::vector<Quaternion> worldRotationDeltas;
};
struct Object3DPoseProgram final
{
    std::vector<std::string> joints;
    std::vector<PoseRigUnit> units;
};
IIACTOROBJECT_EXPORT void validateObject3DPoseProgram(const Object3DPoseProgram &program);
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DPoseProgram makeObject3DPoseProgram(const BvhMotion &motion,
    std::span<const PoseUnitFrame> frames,std::size_t neutralFrame=0,double angularToleranceRadians=0);
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<std::byte> exportObject3DPoseProgram(
    const Object3DPoseProgram &program,std::size_t maximumBytes=64*1024*1024);
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DPoseProgram importObject3DPoseProgram(
    std::span<const std::byte> bytes,std::size_t maximumBytes=64*1024*1024);
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DRigHandle createObject3DPoseRig(const Object3DHandle &object,
    const Object3DPoseProgram &program,std::span<const BoneIndex> jointMapping={});

[[nodiscard]] IIACTOROBJECT_EXPORT std::string object3DPoseMorphName(std::string_view unit);
} // namespace iiActorObject
