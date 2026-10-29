#pragma once
// Private DNA backend construction; independent of editor/project storage.
#include "Object3DDna.h"
namespace iiActorObject::object3d_detail
{
[[nodiscard]] bool isDnaProgramDriver(const Object3DRigDriver &) noexcept;
[[nodiscard]] std::unique_ptr<Object3DRigDriver> makeDnaProgramDriver(
    std::span<const std::byte>,DnaImportOptions);
}
