#pragma once
#include "../third_party/ufbx/ufbx.h"
#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace iiActorObject
{
struct MeshTopology;
namespace object3d_detail
{
struct FbxTangentInspection final
{
    bool present=false;
    std::size_t elementCount=0;
    std::vector<std::string> warnings;
};
struct FbxTangentReadOptions final
{
    std::size_t corners=0,boneCount=0;
    bool reflected=false,restored=false;
    ufbx_mirror_axis mirrorAxis=UFBX_MIRROR_AXIS_NONE;
};
// Parsed source data is borrowed; the inspection and warnings are owned results.
[[nodiscard]] FbxTangentInspection inspectFbxTangents(const ufbx_mesh &source);
// Records an immutable authored tangent snapshot in the supplied topology.
[[nodiscard]] std::vector<std::string> readFbxTangents(const ufbx_mesh &source,const ufbx_matrix &geometry,
    std::span<const ufbx_uv_set *const> additionalUvSets,const FbxTangentInspection &inspection,
    const FbxTangentReadOptions &options,std::size_t &usedCorners,MeshTopology &topology);
}
}
