#pragma once
#include "Object3DExternalIdentity.h"
#include <cstddef>
#include <optional>
#include <string>

struct ufbx_scene;
struct ufbx_node;
struct ufbx_mesh;
namespace iiActorObject
{
struct MeshTopology;
namespace object3d_detail
{
// Borrows parsed FBX data. An absent document or source-mesh property is not
// an error. Restored authoring metadata requires a valid topology layout.
[[nodiscard]] std::string readFbxExternalDocument(const ufbx_scene &scene);
[[nodiscard]] std::optional<ExternalMeshIdentity> readFbxExternalMeshIdentity(
    const ufbx_node &node,const ufbx_mesh &source,const MeshTopology &topology,
    std::size_t mesh,bool restoredAuthoringMetadata,std::size_t maxMorphs);
}
}
