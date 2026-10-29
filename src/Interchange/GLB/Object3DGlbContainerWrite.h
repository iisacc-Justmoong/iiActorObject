#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace iiActorObject::object3d_detail::glb_export
{
// Frames caller-owned JSON and binary bytes into a padded GLB 2.0 container.
[[nodiscard]] std::vector<std::uint8_t> writeGlbContainer(std::string_view json,
                                                           std::span<const std::uint8_t> binary);
} // namespace iiActorObject::object3d_detail::glb_export
