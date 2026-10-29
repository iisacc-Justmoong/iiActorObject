#include "MeshPointWeightValidation.h"
#include <cmath>
#include <stdexcept>

namespace iiActorObject::object3d_detail
{
void validatePointWeights(std::span<const double> values,std::optional<std::size_t> count)
{
    if(count&&values.size()!=*count)throw std::invalid_argument("Point mask dimensions");
    for(const auto value:values)
        if(!std::isfinite(value)||value<0||value>1)throw std::invalid_argument("Point mask weights must be in [0,1]");
}
} // namespace iiActorObject::object3d_detail
