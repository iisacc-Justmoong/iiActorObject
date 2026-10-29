#pragma once
#include <iiActorObject/BoneIndex.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace iiActorObject
{
struct ExternalBoneIdentity final {BoneIndex bone=NoBone,source=NoBone;};
// One identity per logical channel; morph is a representative snapshot record.
struct ExternalMorphIdentity final {std::size_t morph=0;std::string source;};
enum class ExternalMeshTopology : std::uint8_t
{
    SourcePolygons,
    // Face IDs index the canonical source render triangles in groups of three.
    RenderTriangles
};
struct ExternalMeshIdentity final
{
    std::size_t mesh=0,source=0;
    std::vector<std::uint32_t> points,polygons,corners;
    std::vector<ExternalMorphIdentity> morphs;
    ExternalMeshTopology topology=ExternalMeshTopology::SourcePolygons;
};
struct ExternalMaterialIdentity final {std::size_t material=0,source=0;};
struct Object3DExternalIdentity final
{
    std::string document;
    std::vector<ExternalBoneIdentity> bones;
    std::vector<ExternalMeshIdentity> meshes;
    std::vector<ExternalMaterialIdentity> materials;
};
} // namespace iiActorObject
