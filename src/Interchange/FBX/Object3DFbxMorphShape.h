#pragma once
#include "../third_party/ufbx/ufbx.h"
#include "MeshTopologyCore.h"
#include <optional>
#include <vector>

namespace iiActorObject::object3d_detail
{
struct FbxMorphShapeData final
{
    std::vector<TopologyPointDelta> deltas;
    std::optional<std::vector<TopologyNormalDelta>> normalDeltas;
    bool restoredNormalMetadata=false;
};
// All source structures are borrowed; the result owns only the converted deltas.
[[nodiscard]] FbxMorphShapeData readFbxMorphShape(const ufbx_blend_shape &shape,
    const ufbx_mesh &source,const MeshTopology &topology,const ufbx_matrix &geometry,
    const ufbx_dom_node *channelMetadata,bool allowNormalMetadata,ufbx_mirror_axis mirrorAxis);
} // namespace iiActorObject::object3d_detail
