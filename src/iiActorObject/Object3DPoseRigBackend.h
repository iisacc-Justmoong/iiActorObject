#pragma once
#include "Object3DPoseRig.h"

namespace iiActorObject::object3d_detail
{
[[nodiscard]] bool isPoseProgramDriver(const Object3DRigDriver &driver) noexcept;
[[nodiscard]] std::unique_ptr<Object3DRigDriver> makePoseProgramDriver(Object3DPoseProgram program);
[[nodiscard]] std::unique_ptr<Object3DRigDriver> bindPoseProgramDriver(const Object3DData &data,
    const Object3DPoseProgram &program,std::span<const BoneIndex> mapping);
} // namespace iiActorObject::object3d_detail
