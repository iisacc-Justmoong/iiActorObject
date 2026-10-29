#pragma once
#include <iiActorObject/MeshTopologyCore.h>

namespace iiActorObject
{
class SkeletalMesh;
class Pose;
// Expand authored source-point values to the render-vertex order, including seams.
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<double> expandDualQuaternionWeights(
    const MeshTopology &topology,std::span<const double> pointWeights);
// Object-space deformation with one [0,1] DQ blend amount per render vertex.
// Empty factors are not a default: their count must match the selected LOD.
// This computes values only; the mesh, pose and caller's weights are unchanged.
IIACTOROBJECT_EXPORT void deformDualQuaternionSkin(const SkeletalMesh &mesh,const Pose &pose,
    std::size_t lod,std::span<const double> morphWeights,std::span<const double> dualQuaternionWeights,
    std::span<DeformedVertex> destination);
} // namespace iiActorObject
