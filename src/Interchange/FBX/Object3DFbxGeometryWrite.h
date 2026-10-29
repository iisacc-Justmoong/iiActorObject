#pragma once
#include "Object3DFbxBinary.h"
#include "Object3DFbxTangentData.h"
#include <cstdint>
#include <span>

namespace iiActorObject { struct Object3DMesh; }
namespace iiActorObject::object3d_detail
{
// A geometry node is assembled from borrowed mesh and precomputed tangent data.
[[nodiscard]] fbx_binary::Node writeFbxGeometry(const Object3DMesh &mesh,std::int64_t geometry,
    std::span<const FbxTangentLayer> tangentLayers);
} // namespace iiActorObject::object3d_detail
