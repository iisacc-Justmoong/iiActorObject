#pragma once
#include "Object3DFbxBinary.h"
#include "Object3DFbxPoseWrite.h"
#include "Object3DFbxSkinData.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace iiActorObject
{
struct Bone;
namespace object3d_detail
{
struct FbxSkeletonSemantic final
{
    std::size_t bone;
    std::string_view name;
};
using FbxBoneIdentityProperty=fbx_binary::Node (*)(std::size_t);
struct FbxSkeletonWriteInput final
{
    std::span<const Bone> bones;
    std::span<const FbxSkeletonSemantic> semantics;
    std::span<const Matrix4> bindWorlds,inverseBinds;
    std::int64_t rootId;
    // Includes the synthetic rigid root for a static asset, even when bones is empty.
    std::size_t sourceBoneCount,meshCount;
    // Called synchronously twice per bone; null omits external identity.
    FbxBoneIdentityProperty boneIdentity=nullptr;
};
struct FbxSkeletonWriteResult final
{
    fbx_binary::Node pose;
    std::vector<std::int64_t> boneIds;
    std::vector<FbxSkinBone> skinBones;
};
// Inputs are borrowed. Skin-bone names remain valid only while the input bones
// live. On failure, partially written nodes and the ID counter must be discarded.
FbxSkeletonWriteResult writeFbxSkeleton(const FbxSkeletonWriteInput &input,
    std::int64_t &next,fbx_binary::Node &objects,fbx_binary::Node &connections);
}
}
