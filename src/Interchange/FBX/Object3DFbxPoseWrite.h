#pragma once
#include "Object3DFbxBinary.h"
#include <iiActorObject/Transform.h>
#include <cstddef>
#include <cstdint>

namespace iiActorObject::object3d_detail
{
// The count includes source bones, meshes and the object root.
[[nodiscard]] fbx_binary::Node makeFbxBindPose(std::int64_t id,std::size_t sourceBoneCount,std::size_t meshCount);
void appendFbxBindPoseNode(fbx_binary::Node &pose,std::int64_t id,const Matrix4 &matrix);
}
