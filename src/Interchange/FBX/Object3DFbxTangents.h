#pragma once
#include "Object3DFbxTangentData.h"
#include "Object3DFbxTangentProjection.h"
#include <vector>

namespace iiActorObject { struct Object3DData; struct Object3DMesh; }

namespace iiActorObject::object3d_detail
{
[[nodiscard]] std::vector<FbxTangentLayer> buildFbxTangentLayers(const Object3DData &data,const Object3DMesh &mesh);
} // namespace iiActorObject::object3d_detail
