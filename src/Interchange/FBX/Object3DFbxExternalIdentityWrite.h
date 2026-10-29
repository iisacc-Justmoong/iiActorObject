#pragma once
#include "Object3DFbxBinary.h"
#include <cstddef>
#include <string_view>
#include <vector>

namespace iiActorObject
{
struct ExternalMeshIdentity;
struct MeshTopology;
namespace object3d_detail
{
// The returned properties borrow no data. A source layout, when supplied,
// must remain valid during the call and determines visible face/corner order.
fbx_binary::Node fbxExternalDocumentProperty(std::string_view document);
fbx_binary::Node fbxExternalBoneProperty(std::size_t sourceIndex);
std::vector<fbx_binary::Node> fbxExternalMeshProperties(std::string_view document,
    std::size_t meshIndex,const ExternalMeshIdentity &identity,const MeshTopology *source);
}
}
