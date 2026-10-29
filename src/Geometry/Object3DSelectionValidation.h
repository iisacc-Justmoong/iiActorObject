#pragma once
// Private selection validation interface. Data belongs to the core engine.
#include "Object3DSelectionData.h"
#include <optional>
#include <span>

namespace iiActorObject { struct Object3DData; }
namespace iiActorObject::object3d_detail
{
void validatePointSelectionState(const Object3DData &data,const PointSelectionState &state);
// State contents are already validated and immutable; only mesh dimensions need rechecking.
void validatePointSelectionShape(const Object3DData &data,const PointSelectionState &state);
void validatePointSelectionTransition(const Object3DData &before,const Object3DData &after,
                                      const PointSelectionState &state);
enum class SelectionRemapMode { Removal, Insertion, Mixed };
// Validates the exact retained selection rows and source identities.
void validateRemappedPointSelectionTransition(const Object3DData &before,const Object3DData &after,
    const PointSelectionState &previous,const PointSelectionState &next,
    std::span<const std::optional<std::size_t>> oldToNew,SelectionRemapMode mode=SelectionRemapMode::Removal);
} // namespace iiActorObject::object3d_detail
