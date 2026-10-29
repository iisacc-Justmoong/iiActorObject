#pragma once
#include "Object3DGlbJson.h"
#include <cstdint>
#include <string>
#include <vector>

struct ufbx_matrix;
namespace iiActorObject { class GlbDocument; struct MeshTopology; struct MeshTopologyDefinition; }
namespace iiActorObject::object3d_detail::glb_import
{
struct GeometryBudgets;
struct Skin;
// Reads the optional sidecar, maps source bones, and publishes only a complete plan.
bool restorePointIdentity(const Value &root,const Value &metadata,const GlbDocument &document,
    const GlbImportOptions &options,GeometryBudgets &budget,const Skin &skin,
    const ufbx_matrix &transform,MeshTopology &topology,MeshTopologyDefinition &definition,
    const std::vector<std::uint32_t> &ids,bool invalid,bool externalProvenance,
    std::vector<std::string> &warnings);
}
