#pragma once
#include "Object3DSelectionData.h"
#include <cstdint>
#include <optional>
#include <span>

namespace iiActorObject { struct Object3DData; }
namespace iiActorObject::object3d_detail
{
// Values are borrowed only for the validation call. A missing selection is
// represented by nullptr; shared selection identity is pointer equality.
struct ProjectHistorySelectionView final
{
    const Object3DData &object;
    const PointSelectionState *selection;
    bool layered;
};
// sameObject is the caller's engine-level identity comparison. No values are
// changed or published, even if an invalid transition is rejected.
void validateProjectHistorySelectionTransition(std::uint32_t version,
    const ProjectHistorySelectionView *previous,const ProjectHistorySelectionView &current,
    bool sameObject,std::span<const std::optional<std::size_t>> remapping);
}
