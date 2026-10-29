#pragma once
#include "Object3DMesh.h"
#include <cstddef>
#include <optional>
#include <set>
#include <span>
#include <vector>

namespace iiActorObject::object3d_detail
{
// Inputs are borrowed and never modified. A visibility change requires the
// caller to validate retained topology compatibility before assembly.
[[nodiscard]] Object3DMesh templateSurfaceRetainedMesh(const Object3DMesh &old,
    const Object3DMesh &candidate,bool changeVisibility);
// newToOld maps each candidate slot to a retained source slot, or no value for
// an added slot. Retained slots must be unique and in their original order.
[[nodiscard]] std::vector<Object3DMesh> assembleTemplateSurfaceMeshes(
    std::span<const Object3DMesh> before,std::span<const Object3DMesh> after,
    std::span<const std::optional<std::size_t>> newToOld,
    const std::set<std::size_t> &added,const std::set<std::size_t> &visibility);
// The candidate must first pass the single-material validation. Keeps the
// existing material slot and rejects remapping authored tangents.
[[nodiscard]] Object3DMesh templateSurfaceReplacementMesh(const Object3DMesh &candidate,
    std::size_t existingMaterial);
} // namespace iiActorObject::object3d_detail
