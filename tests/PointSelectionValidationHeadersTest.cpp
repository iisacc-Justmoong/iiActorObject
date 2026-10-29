#include "../src/Geometry/Object3DSelectionValidation.h"
#include "../src/Geometry/MeshPointWeightValidation.h"
#include <type_traits>

using WeightValidator=void (*)(std::span<const double>,std::optional<std::size_t>);
static_assert(std::is_same_v<decltype(&iiActorObject::object3d_detail::validatePointWeights),WeightValidator>);
