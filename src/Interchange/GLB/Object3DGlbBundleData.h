#pragma once
#include <cstddef>
#include <cstdint>

namespace iiActorObject::object3d_detail::glb_import
{
struct GlbMeshBundle final {std::uint32_t layoutOffset=0;std::size_t count=0;};
struct GlbPointBundle final {std::uint32_t pointOffset=0;std::size_t count=0;};
}
