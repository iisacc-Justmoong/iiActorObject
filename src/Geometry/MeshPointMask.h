#pragma once
#include <cstddef>
#include <vector>

namespace iiActorObject
{
struct MeshPointMask final
{
    std::size_t mesh=0;
    std::vector<double> weights;
    bool operator==(const MeshPointMask&)const=default;
};
} // namespace iiActorObject
