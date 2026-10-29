#pragma once
#include <array>
#include <cstddef>

namespace iiActorObject::object3d_detail::glb_export
{
// Source material index, base-color UV index and normal-map UV index.
using GlbMaterialVariant=std::array<std::size_t,3>;
} // namespace iiActorObject::object3d_detail::glb_export
