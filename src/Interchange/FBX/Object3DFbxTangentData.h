#pragma once
#include "Transform.h"
#include <cstddef>
#include <string_view>
#include <vector>

namespace iiActorObject::object3d_detail
{
struct FbxTangentLayer final
{
    std::size_t uvIndex=0;
    std::string_view uvName;
    std::vector<Vector3> tangents,binormals;
    std::vector<double> handedness, binormalW;
    // Empty means a complete base layer. Other values describe a fallback
    // or that shape tangent deltas are not represented.
    std::string_view status;
};
} // namespace iiActorObject::object3d_detail
