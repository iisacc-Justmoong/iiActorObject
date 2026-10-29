#pragma once
#include "../third_party/ufbx/ufbx.h"
#include <iiActorObject/MeshData.h>
#include <unordered_map>
#include <vector>

namespace iiActorObject::object3d_detail
{
struct FbxSkinData final
{
    SkinningProfile profile;
    std::vector<std::vector<SkinWeight>> weights;
};
// The source node and bone map borrow a parsed scene. No actor or engine is created.
[[nodiscard]] FbxSkinData readFbxSkin(const ufbx_mesh &source,const ufbx_node &node,
    const std::unordered_map<const ufbx_node *,BoneIndex> &bones,std::size_t maximumVertices);
} // namespace iiActorObject::object3d_detail
