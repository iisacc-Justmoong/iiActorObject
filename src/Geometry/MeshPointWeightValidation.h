#pragma once
#include <cstddef>
#include <optional>
#include <span>

namespace iiActorObject::object3d_detail
{
void validatePointWeights(std::span<const double> values,std::optional<std::size_t> count={});
} // namespace iiActorObject::object3d_detail
