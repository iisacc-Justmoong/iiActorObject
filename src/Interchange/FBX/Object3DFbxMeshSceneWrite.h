#pragma once
#include "Object3DFbxBinary.h"
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace iiActorObject::object3d_detail
{
struct FbxMeshSceneInput final
{
    std::string_view name;
    std::int64_t geometryId,modelId;
    std::span<const std::int64_t> materialIds;
    std::vector<fbx_binary::Node> identityProperties;
};
// Inputs are borrowed except the moved property values. On failure, discard
// all partially updated staging nodes rather than publishing them.
void appendFbxMeshScene(FbxMeshSceneInput input,fbx_binary::Node &objects,
    fbx_binary::Node &connections,fbx_binary::Node &pose);
}
