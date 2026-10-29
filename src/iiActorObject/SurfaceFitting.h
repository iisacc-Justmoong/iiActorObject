#pragma once
#include <iiActorObject/MeshEditing.h>
#include <iiActorObject/SurfaceCollider.h>
#include <cstdint>

namespace iiActorObject
{
enum class SurfaceFairingShapePolicy : std::uint32_t
{
    LegacyDisplacement=0,
    PreserveMaximumEdgeStrain=1
};
struct SurfaceFairingOptions final
{
    unsigned iterations=0;
    double strength=.5,referenceWeight=.01;
    unsigned projectionIterations=12,lineSearchSteps=5;
    SurfaceFairingShapePolicy shapePolicy=SurfaceFairingShapePolicy::PreserveMaximumEdgeStrain;
    bool operator==(const SurfaceFairingOptions &) const=default;
};
struct SurfaceFairingResult final
{
    unsigned iterations=0,accepted=0,rejected=0;
    double energyBefore=0,energyAfter=0;
};
struct SurfaceFitOptions final
{
    double clearance=.002,tolerance=1e-6,relaxation=1,edgeStiffness=.1;
    double maximumDisplacement=.25;
    unsigned iterations=64;
    std::span<const double> mobility; // Normalized inverse masses in [0,1]; zero pins a source point.
    bool includeUnusedPoints=false,checkTriangles=true,allowUnresolved=false;
    std::size_t maximumPoints=4'000'000,maximumPointVisits=64'000'000;
    std::size_t maximumTriangles=4'000'000;
    std::size_t maximumNodeVisits=128'000'000,maximumTriangleTests=128'000'000;
    bool escapeConcavities=true;
    SurfaceFairingOptions fairing;
};
struct SurfaceFitResult final
{
    bool converged=false;
    bool trianglesChecked=false;
    unsigned iterations=0;
    std::size_t changedPoints=0,vertexProjections=0,triangleProjections=0;
    std::size_t pointViolations=0,triangleViolations=0,pinnedViolations=0,clampedPoints=0;
    std::size_t pointVisits=0,nodeVisits=0,triangleTests=0;
    double maximumViolationBefore=0,maximumViolationAfter=0;
    double minimumVertexClearance=std::numeric_limits<double>::infinity();
    double minimumTriangleClearance=0; // Capped at the requested clearance; not an unbounded distance.
    double maximumDisplacement=0,maximumEdgeStrain=0;
    std::size_t escapeTrials=0,escapePointProjections=0,escapeTriangleProjections=0;
    SurfaceFairingResult fairing;
};
IIACTOROBJECT_EXPORT void validateSurfaceFitOptions(const SurfaceFitOptions &options,
    std::size_t sourcePointCount,std::size_t triangleCount);
// Explicit snapshot fit. Preserves topology IDs, UVs, skin and morph identities.
// Fails atomically on unresolved constraints unless preview output is explicitly enabled.
[[nodiscard]] IIACTOROBJECT_EXPORT SurfaceFitResult fitMeshSurface(Object3DMesh &mesh,
    std::span<const SurfaceCollider *const> obstacles,const SurfaceFitOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT SurfaceFitResult fitMeshSurface(Object3DMesh &mesh,
    const SurfaceCollider &obstacle,const SurfaceFitOptions &options={});
} // namespace iiActorObject
