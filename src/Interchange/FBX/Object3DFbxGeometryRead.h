#pragma once
#include "../third_party/ufbx/ufbx.h"
#include "Object3DMesh.h"
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace iiActorObject::object3d_detail
{
struct FbxGeometryReadLimits final
{
    std::size_t maxVertices,maxPolygons,maxCorners,maxUvSets,maxUvEntries;
    bool restoreMetadata;
};
struct FbxGeometryReadBudget final
{
    std::size_t points=0,faces=0,corners=0,renderVertices=0,uvEntries=0;
};
struct FbxGeometryReadResult final
{
    Object3DMesh mesh;
    MeshTopology topology;
    ufbx_matrix geometry{};
    bool reflected=false;
    std::size_t corners=0;
    // These pointers borrow the parsed scene until the mesh import is complete.
    const ufbx_dom_node *metadata=nullptr;
    std::vector<const ufbx_uv_set*> extraUvSets;
    std::vector<std::string> warnings;
};
// Builds an unpublished mesh snapshot and charges the budget only on success.
[[nodiscard]] FbxGeometryReadResult readFbxGeometry(const ufbx_mesh &source,const ufbx_node &node,
    const std::unordered_map<const ufbx_node*,BoneIndex> &bones,
    const std::unordered_map<const ufbx_material*,std::size_t> &materials,
    std::string name,const FbxGeometryReadLimits &limits,FbxGeometryReadBudget &budget);
}
