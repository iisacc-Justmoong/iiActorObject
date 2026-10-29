#pragma once
#include "../third_party/ufbx/ufbx.h"
#include "MeshTopologyCore.h"
#include <cstddef>

namespace iiActorObject::object3d_detail
{
// Limits apply to one decoded authoring-topology metadata node.
struct FbxTopologyReadLimits final
{
    std::size_t maxCorners,maxPolygons,maxVertices;
};
[[nodiscard]] MeshTopologyDefinition readFbxTopologyMetadata(const ufbx_dom_node *node,
                                                              const FbxTopologyReadLimits &limits);
} // namespace iiActorObject::object3d_detail
