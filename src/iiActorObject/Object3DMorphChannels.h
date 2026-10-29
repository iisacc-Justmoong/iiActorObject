#pragma once
#include "Object3DAsset.h"
#include <span>

namespace iiActorObject::object3d_detail
{
// Portable projection only. The source records and their IDs remain in the asset.
struct LogicalMorphGroup final
{
    TopologyMorph morph;
    std::size_t representative=0;
    std::size_t recordCount=0;
};
[[nodiscard]] std::vector<LogicalMorphGroup> logicalMorphGroups(const Object3DMesh &mesh);
[[nodiscard]] std::vector<LogicalMorphGroup> logicalMorphGroups(const Object3DMesh &mesh,
    std::span<const TopologyMorph> records);
} // namespace iiActorObject::object3d_detail
