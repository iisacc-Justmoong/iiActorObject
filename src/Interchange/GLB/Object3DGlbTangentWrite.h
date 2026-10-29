#pragma once
#include <cstddef>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace iiActorObject
{
struct Object3DMesh;
namespace object3d_detail
{
struct GlbTangentPlan;
namespace glb_export
{
struct Buffer;
void writeGlbTangentExtras(std::ostream &out,std::string_view fallback,std::string_view authoredStatus,
    std::span<const std::string_view> fbxNotes={},std::optional<std::size_t> fbxUv={});
// The caller owns inputs and discards the temporary buffer if serialization throws.
[[nodiscard]] std::string writeGlbTangentPrimitive(Buffer &buffer,const Object3DMesh &mesh,
    const GlbTangentPlan &plan,bool hasRig,std::size_t influenceSets,std::size_t material,
    std::string_view authoredStatus={},std::span<const std::string_view> fbxNotes={},
    std::optional<std::size_t> fbxUv={});
}
}
} // namespace iiActorObject
