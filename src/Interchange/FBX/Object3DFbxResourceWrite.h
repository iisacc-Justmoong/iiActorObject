#pragma once
#include "Object3DFbxBinary.h"
#include "Object3DImage.h"
#include "Object3DMaterial.h"
#include <cstdint>
#include <span>
#include <vector>

namespace iiActorObject::object3d_detail
{
// The caller owns IDs and nodes; the returned IDs follow source material order.
[[nodiscard]] std::vector<std::int64_t> writeFbxResources(
    std::span<const Object3DImage> images,std::span<const Object3DMaterial> materials,
    bool includeExternalIdentity,std::int64_t &next,fbx_binary::Node &objects,fbx_binary::Node &connections);
} // namespace iiActorObject::object3d_detail
