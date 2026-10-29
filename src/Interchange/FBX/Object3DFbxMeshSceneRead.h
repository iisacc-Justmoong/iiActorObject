#pragma once
#include "Object3DFbxGeometryRead.h"
#include "Object3DFbxImportFields.h"
#include "Object3DExternalIdentity.h"

namespace iiActorObject::object3d_detail
{
class FbxMorphChannelReader;
struct FbxMeshSceneReadOptions final
{
    FbxGeometryReadLimits geometry;
    std::size_t maximumMorphs,boneCount;
    bool readExternalIdentity;
};

// The scene and lookup maps are borrowed. Meshes and source IDs are staged;
// if reading fails the caller discards the reader and its other staged outputs.
[[nodiscard]] std::vector<Object3DMesh> readFbxMeshScene(
    const ufbx_scene &scene,const std::unordered_map<const ufbx_node*,BoneIndex> &bones,
    const std::unordered_map<const ufbx_material*,std::size_t> &materials,
    FbxMorphChannelReader &morphs,std::vector<ExternalMeshIdentity> &identities,
    const FbxMeshSceneReadOptions &options,const FbxWarningSink &warn);
}
