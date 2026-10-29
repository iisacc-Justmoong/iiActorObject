#pragma once
#include "MeshTopologyCore.h"
#include "Object3DFbxBinary.h"
#include "Object3DFbxSkinData.h"
#include <cstdint>
#include <span>
#include <string_view>

namespace iiActorObject::object3d_detail
{
// Bone names and input rows are borrowed only for the duration of the call.
void writeFbxSkin(std::string_view name,const SkinningProfile &profile,std::span<const TopologyControlPoint> points,
    std::span<const FbxSkinBone> bones,std::int64_t skinId,std::int64_t geometryId,std::int64_t &next,
    fbx_binary::Node &objects,fbx_binary::Node &connections);
void writeFbxSkin(std::string_view name,const SkinningProfile &profile,std::span<const SkinVertex> vertices,
    std::span<const FbxSkinBone> bones,std::int64_t skinId,std::int64_t geometryId,std::int64_t &next,
    fbx_binary::Node &objects,fbx_binary::Node &connections);
} // namespace iiActorObject::object3d_detail
