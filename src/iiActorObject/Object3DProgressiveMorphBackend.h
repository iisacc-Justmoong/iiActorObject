#pragma once
#include "Object3DProgressiveMorph.h"

namespace iiActorObject::object3d_detail
{
[[nodiscard]] bool isProgressiveMorphRigDriver(const Object3DRigDriver &driver) noexcept;
[[nodiscard]] bool isProgressiveMorphRigSession(const Object3DRigSession &session) noexcept;
[[nodiscard]] const ProgressiveMorphProgram &progressiveMorphRigProgram(const Object3DRigDriver &driver);
[[nodiscard]] std::unique_ptr<Object3DRigDriver> makeProgressiveMorphRigDriver(
    const ProgressiveMorphProgram &program,const Object3DAsset &asset);
}
