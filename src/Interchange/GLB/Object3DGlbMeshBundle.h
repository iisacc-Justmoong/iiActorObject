#pragma once
#include "Object3DGlbBundleData.h"
#include "Object3DGlbGeometryBudget.h"
#include "Object3DGlbMeshAttributes.h"
#include <iiActorObject/BoneIndex.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

struct ufbx_matrix;
namespace iiActorObject { class GlbDocument; struct MeshTopology; struct MeshTopologyDefinition; struct TopologyGlbTangentSource; }
namespace iiActorObject::object3d_detail::glb_import
{
struct GlbUvFallbacks final
{
    bool hasPrimary=false;
    std::optional<std::uint32_t> primary;
    std::vector<std::optional<std::uint32_t>> additional;
};
struct GlbMeshSkinView final
{
    bool present=false;
    BoneIndex rigidBone=NoBone;
    std::span<const BoneIndex> sourceToBone;
};
// Appends a single decoded bundle to caller-owned staging data. On failure the
// caller discards the staging data and work counters; no engine is owned here.
GlbMeshBundle appendGlbMeshBundle(const GlbDocument &document,const AttributeSet &attributes,
    const std::vector<Target> &targets,const GlbMeshSkinView &skin,const ufbx_matrix &transform,
    bool quantized,const GlbImportOptions &options,GeometryBudgets &budget,MeshTopology &topology,
    MeshTopologyDefinition &definition,TopologyGlbTangentSource &authored,
    std::optional<GlbPointBundle> sharedPoints,std::vector<std::uint32_t> &sourcePointIds,
    bool &identityInvalid,GlbUvFallbacks &fallbacks);
}
