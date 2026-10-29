#pragma once
#include <cstddef>
#include <limits>

namespace iiActorObject
{
using BoneIndex = std::size_t;
inline constexpr BoneIndex NoBone = std::numeric_limits<BoneIndex>::max();
} // namespace iiActorObject
