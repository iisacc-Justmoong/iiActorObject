#pragma once
#include <memory>
#include <span>

namespace iiActorObject { struct MeshTopology; struct TopologyGlbTangentBinding; struct TopologyGlbTangentSource; }
namespace iiActorObject::object3d_detail::glb_import
{
// Produces immutable source tangent metadata without changing the input topology.
[[nodiscard]] std::shared_ptr<const TopologyGlbTangentSource> makeGlbTangentSnapshot(
    const MeshTopology &topology,TopologyGlbTangentSource authored,
    std::span<const TopologyGlbTangentBinding> bindings);
}
