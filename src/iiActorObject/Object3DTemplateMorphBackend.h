#pragma once
#include "Object3DTemplateMorphs.h"
#include "Object3DRig.h"

namespace iiActorObject::object3d_detail
{
[[nodiscard]] bool isTemplateMorphRigDriver(const Object3DRigDriver &driver) noexcept;
[[nodiscard]] const TemplateMorphRigInfo &templateMorphRigInfo(const Object3DRigDriver &driver);
[[nodiscard]] std::unique_ptr<Object3DRigDriver> makeTemplateMorphRigDriver(TemplateMorphRigInfo info);
} // namespace iiActorObject::object3d_detail
