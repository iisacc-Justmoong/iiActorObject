#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace iiActorObject
{
struct MeshTopology;
struct MeshTopologyDefinition;
namespace object3d_detail::glb_import
{
// Generates face-specific layouts where an imported primitive lacks normals.
std::size_t generateGlbFlatNormals(MeshTopology &topology,MeshTopologyDefinition &definition,
    std::size_t &flatLayoutWork,std::span<const std::uint8_t> missingFaces);
// Derives corresponding normal deltas after the stable topology has been created.
void generateGlbFlatMorphNormals(MeshTopology &topology,std::span<const std::uint8_t> missingFaces,
    std::size_t generatedLayouts,std::size_t maximumMorphWork,std::size_t &flatMorphWork);
}
}
