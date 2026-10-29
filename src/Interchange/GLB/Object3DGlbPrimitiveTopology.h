#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace iiActorObject { struct MeshTopologyDefinition; struct TopologyControlPoint; }
namespace iiActorObject::object3d_detail::glb_import
{
// Validate a whole primitive before appending any face, material or normal flag.
void appendGlbPrimitiveTriangles(std::span<const std::uint32_t> indices,
    std::size_t layoutOffset,std::size_t vertexCount,std::span<const TopologyControlPoint> points,
    bool skinned,std::size_t material,bool hasNormals,MeshTopologyDefinition &definition,
    std::vector<std::uint8_t> &missingNormalFaces);
}
