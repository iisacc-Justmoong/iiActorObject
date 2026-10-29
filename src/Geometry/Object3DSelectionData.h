#pragma once
#include "MeshPointMask.h"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

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
} // namespace iiActorObject::object3d_detail
