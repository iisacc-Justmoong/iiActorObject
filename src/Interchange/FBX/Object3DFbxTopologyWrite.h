#pragma once
#include "Object3DFbxBinary.h"
#include "MeshTopologyCore.h"

namespace iiActorObject::object3d_detail
{
// Creates the versioned authoring metadata; the caller owns the FBX scene.
[[nodiscard]] fbx_binary::Node writeFbxTopologyMetadata(const MeshTopology &topology);
} // namespace iiActorObject::object3d_detail
