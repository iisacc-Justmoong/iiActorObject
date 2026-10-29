#pragma once
#include "Object3DGlbTangentData.h"
#include "Object3DMesh.h"
#include <optional>
#include <string_view>

namespace iiActorObject::object3d_detail
{
[[nodiscard]] std::optional<GlbTangentPlan> planGlbTangents(const Object3DMesh &mesh,
    std::span<const std::uint32_t> cornerIndices,std::span<const TextureCoordinate> uv,
    std::string_view &fallbackReason);
// A valid authored snapshot has already been checked against the mesh and
// material by the caller. The work budget is shared by all of its primitives.
[[nodiscard]] GlbTangentPlan planAuthoredGlbTangents(const Object3DMesh &mesh,
    std::span<const std::uint32_t> cornerIndices,std::span<const std::uint32_t> tangentByLayout,
    std::size_t &work);
// Split at whole-triangle boundaries so one unrepresentable FBX basis does not
// discard representable authored data on other faces of the same material.
struct GlbFbxTangentSplit final
{
    GlbTangentPlan authored;
    std::vector<std::uint32_t> fallbackCorners;
    std::vector<std::string_view> authoredNotes,fallbackNotes;
};
// A valid FBX snapshot and visible polygon-corner projection are supplied by
// the caller. Notes describe data that glTF cannot represent exactly.
[[nodiscard]] GlbFbxTangentSplit planFbxGlbTangents(const Object3DMesh &mesh,
    const TopologyTangentSourceLayer &layer,std::span<const std::uint32_t> cornerIndices,
    std::span<const std::uint32_t> triangleToPolygon,std::span<const std::string_view> commonNotes);
} // namespace iiActorObject::object3d_detail
