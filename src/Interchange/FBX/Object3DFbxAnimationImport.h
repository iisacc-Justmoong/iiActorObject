#pragma once
#include "Object3DFbxAnimationData.h"
#include "Object3DFbxAnimationStage.h"
#include "Object3DInterchange.h"
#include "../third_party/ufbx/ufbx.h"
#include <map>

namespace iiActorObject::object3d_detail
{
struct FbxImportedNode final
{
    const ufbx_node *node;
    const ufbx_node *parent;
    BoneIndex bone;
};
std::vector<FbxImportedClip> planFbxImportedAnimations(const ufbx_scene &scene,
    std::span<const FbxImportedNode> nodes, const FbxImportedMorphs &morphs,
    std::span<const FbxImportedProgressiveControl> progressive,
    const FbxAnimationImportOptions &options, std::vector<std::string> &warnings);
} // namespace iiActorObject::object3d_detail
