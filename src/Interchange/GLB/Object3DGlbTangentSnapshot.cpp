#include "Object3DGlbTangentSnapshot.h"
#include "MeshTopologyCore.h"
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail::glb_import
{
std::shared_ptr<const TopologyGlbTangentSource> makeGlbTangentSnapshot(const MeshTopology &topology,
    TopologyGlbTangentSource authored,std::span<const TopologyGlbTangentBinding> bindings)
{
    if(authored.tangents.empty())return {};
    if(!topology.layout)throw std::invalid_argument("GLB tangent snapshot requires a topology layout");
    authored.definition=topology.layout->definition();
    authored.triangles.assign(topology.layout->triangles().begin(),topology.layout->triangles().end());
    authored.trianglePolygons.assign(topology.layout->trianglePolygons().begin(),topology.layout->trianglePolygons().end());
    authored.positions.clear();authored.positions.reserve(topology.points.size());
    for(const auto &point:topology.points)authored.positions.push_back(point.position);
    authored.normals=topology.normals;authored.textureCoordinates=topology.textureCoordinates;
    authored.primaryUvName=topology.primaryUvName;authored.additionalUvSets=topology.additionalUvSets;
    authored.morphTargets=topology.morphTargets;
    authored.bindings.assign(bindings.begin(),bindings.end());
    return std::make_shared<const TopologyGlbTangentSource>(std::move(authored));
}
}
