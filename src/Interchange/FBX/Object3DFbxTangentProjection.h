#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace iiActorObject { struct Object3DMesh; }
namespace iiActorObject::object3d_detail
{
// Map render-triangle corners to visible FBX polygon corners, preserving winding.
struct FbxTangentProjection final
{
    std::vector<std::uint32_t> triangleToPolygon;
    std::size_t cornerCount=0;
};
[[nodiscard]] std::optional<FbxTangentProjection> projectFbxTangentCorners(
    const Object3DMesh &mesh,std::string_view &failure);
} // namespace iiActorObject::object3d_detail
