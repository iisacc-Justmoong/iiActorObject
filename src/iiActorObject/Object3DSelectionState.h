#pragma once
// Private, immutable editor history state. The core engine owns these records.
#include "MeshEditing.h"

namespace iiActorObject::object3d_detail
{
inline constexpr std::size_t MaximumPointSelections=1024;
inline constexpr std::size_t MaximumPointSelectionWeights=16'000'000;
struct PointSelectionRecord final
{
    std::string name;
    std::shared_ptr<const std::vector<MeshPointMask>> masks;
};
struct PointSelectionState final
{
    std::vector<PointSelectionRecord> entries;
};
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
