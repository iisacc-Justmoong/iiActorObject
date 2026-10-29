#pragma once
#include "Object3DAsset.h"
#include <optional>
#include <string_view>

namespace iiActorObject::object3d_detail
{
// Map each render-triangle corner to the corresponding visible FBX polygon
// corner. Both interchange directions must use the same winding convention.
struct FbxTangentProjection final
{
    std::vector<std::uint32_t> triangleToPolygon;
    std::size_t cornerCount=0;
};
[[nodiscard]] std::optional<FbxTangentProjection> projectFbxTangentCorners(
    const Object3DMesh &mesh,std::string_view &failure);
struct FbxTangentLayer final
{
    std::size_t uvIndex=0;
    std::string_view uvName;
    std::vector<Vector3> tangents,binormals;
    std::vector<double> handedness, binormalW;
    // Empty means a complete base layer. Other values describe a fallback
    // or that shape tangent deltas are not represented.
    std::string_view status;
};
[[nodiscard]] std::vector<FbxTangentLayer> buildFbxTangentLayers(const Object3DData &data,const Object3DMesh &mesh);
} // namespace iiActorObject::object3d_detail
