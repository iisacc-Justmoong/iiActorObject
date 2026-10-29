#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace iiActorObject
{
struct Object3DMaterial;
struct Object3DMesh;
namespace object3d_detail::glb_export
{
struct GlbTrianglePart final
{
    std::vector<std::uint32_t> vertices,corners;
    // Status strings returned by the planner refer to static storage.
    std::string_view authoredStatus;
};
struct GlbPrimitivePlan final
{
    std::map<std::pair<std::size_t,bool>,GlbTrianglePart> parts;
    std::vector<std::uint32_t> tangentByLayout;
};
// Groups triangles by material and authored tangent availability without mutating input.
[[nodiscard]] GlbPrimitivePlan planGlbPrimitives(const Object3DMesh &mesh,
                                                  std::span<const Object3DMaterial> materials);
}
} // namespace iiActorObject
