#pragma once
#include "MeshTangents.h"
#include <cstdint>
#include <vector>

namespace iiActorObject::object3d_detail
{
// One entry per exported corner; morph tangent XYZ deltas use MorphDelta::normal.
struct GlbTangentPlan final
{
    std::vector<std::uint32_t> sourceVertices;
    std::vector<MeshCornerTangent> tangents;
    std::vector<MorphTarget> morphTangents;
};
} // namespace iiActorObject::object3d_detail
