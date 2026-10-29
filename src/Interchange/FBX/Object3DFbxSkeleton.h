#pragma once
#include "Object3DFbxImportFields.h"
#include <iiActorObject/Object3DData.h>
#include <unordered_map>
#include <utility>

namespace iiActorObject::object3d_detail
{
struct FbxSkeletonReadOptions final
{
    std::size_t maxBones;
    double maximumJointMatrixError;
    Object3DKind kind;
    bool includeExternalIds;
};
struct FbxSkeletonData final
{
    Object3DKind kind;
    std::vector<Bone> bones;
    std::vector<Object3DJointSemantic> semantics;
    // Node keys and parents borrow the parsed scene for the duration of import.
    std::vector<const ufbx_node *> ordered;
    std::unordered_map<const ufbx_node *,BoneIndex> boneIndex;
    std::unordered_map<const ufbx_node *,const ufbx_node *> parents;
    std::vector<std::pair<std::size_t,std::uint32_t>> externalBoneIds;
    double maximumJointMatrixError=0;
};
[[nodiscard]] FbxSkeletonData readFbxSkeleton(const ufbx_scene &scene,
    const FbxSkeletonReadOptions &options,const FbxWarningSink &warn);
} // namespace iiActorObject::object3d_detail
