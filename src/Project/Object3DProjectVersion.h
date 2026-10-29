#pragma once
#include <cstdint>

namespace iiActorObject::object3d_detail
{
struct ProjectArchive;
// Selects the earliest writer version (8 or later) that represents this graph.
[[nodiscard]] std::uint32_t selectProjectVersion(const ProjectArchive &graph);
}
