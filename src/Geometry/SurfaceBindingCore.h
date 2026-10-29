#pragma once
#include <iiActorObject/Transform.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace iiActorObject
{
struct Object3DData;
struct SurfaceBindingPoint final
{
    std::uint32_t point=0,triangle=0;
    std::array<std::uint32_t,3> vertices{};
    std::array<double,3> barycentric{};
    Vector3 frameOffset;
};
enum class SurfaceBindingMode {TriangleFrame,ControlPoints};
struct SurfaceBindingAxisScale final
{
    std::uint32_t a=0,b=0;
    double referenceSpan=1;
};
struct SurfaceControlPoint final
{
    std::uint32_t point=0;
    std::array<std::uint32_t,3> vertices{};
    std::array<double,3> weights{};
    Vector3 offset;
};
struct SurfaceBinding final
{
    std::size_t sourceMesh=0,targetMesh=0,sourceVertexCount=0,targetPointCount=0;
    std::vector<SurfaceBindingPoint> points;
    SurfaceBindingMode mode=SurfaceBindingMode::TriangleFrame;
    std::array<std::optional<SurfaceBindingAxisScale>,3> offsetScales{};
};
// Columns map authored control-point offset axes into object space.
// Span measurements use the inverse basis. A missing basis means object axes.
using SurfaceBindingBasis=std::array<Vector3,3>;
struct SurfaceBindingOptions final
{
    double maximumDistance=std::numeric_limits<double>::infinity();
    bool includeUnusedPoints=false;
};
struct SurfaceBindingLimits final
{
    // Additional target source-point IDs, e.g. hidden anchor helpers.
    std::span<const std::uint32_t> additionalPoints;
    std::size_t maximumVertices=4'000'000,maximumTriangles=4'000'000,maximumPoints=2'000'000;
    std::size_t maximumNodeVisits=128'000'000,maximumTriangleTests=128'000'000;
};
// Captures a topology-preserving surface follower in the common object coordinate system.
[[nodiscard]] IIACTOROBJECT_EXPORT SurfaceBinding bindMeshSurface(const Object3DData &data,
    std::size_t sourceMesh,std::size_t targetMesh,const SurfaceBindingOptions &options={});
// Extended bounded form; the original options and overload remain unchanged.
[[nodiscard]] IIACTOROBJECT_EXPORT SurfaceBinding bindMeshSurface(const Object3DData &data,
    std::size_t sourceMesh,std::size_t targetMesh,const SurfaceBindingOptions &options,const SurfaceBindingLimits &limits);
// Authored mappings use canonical control-point IDs, including hidden helpers.
// Signed coefficients are retained; offsets use object axes and optional spans.
[[nodiscard]] IIACTOROBJECT_EXPORT SurfaceBinding bindMeshControlPoints(const Object3DData &data,
    std::size_t sourceMesh,std::size_t targetMesh,std::span<const SurfaceControlPoint> points,
    const std::array<std::optional<SurfaceBindingAxisScale>,3> &scales={});
// Construct authored mappings when their axes differ from object space.
[[nodiscard]] IIACTOROBJECT_EXPORT SurfaceBinding bindMeshControlPointsWithBasis(const Object3DData &data,
    std::size_t sourceMesh,std::size_t targetMesh,std::span<const SurfaceControlPoint> points,
    const std::array<std::optional<SurfaceBindingAxisScale>,3> &scales,const SurfaceBindingBasis &basis);
// Returns absolute positions in binding.points order without changing the asset.
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<Vector3> evaluateMeshSurfaceBinding(
    const Object3DData &data,const SurfaceBinding &binding);
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<Vector3> evaluateMeshSurfaceBindingWithBasis(
    const Object3DData &data,const SurfaceBinding &binding,const SurfaceBindingBasis &basis);
// Initial fitting replaces the mapped target points; later follower evaluation
// adds only source changes, retaining independently authored target corrections.
IIACTOROBJECT_EXPORT void fitMeshSurfaceBinding(Object3DData &data,const SurfaceBinding &binding);
IIACTOROBJECT_EXPORT void fitMeshSurfaceBindingWithBasis(Object3DData &data,const SurfaceBinding &binding,
    const SurfaceBindingBasis &basis);
IIACTOROBJECT_EXPORT void validateMeshSurfaceBindings(const Object3DData &data,std::span<const SurfaceBinding> bindings);
IIACTOROBJECT_EXPORT void validateMeshSurfaceBindingsWithBases(const Object3DData &data,
    std::span<const SurfaceBinding> bindings,std::span<const std::optional<SurfaceBindingBasis>> bases);
// Adds source-surface changes to existing target positions, retaining target sculpt/shape corrections.
// Bindings form an acyclic graph with one incoming binding per target mesh. Strong exception guarantee.
IIACTOROBJECT_EXPORT void applyMeshSurfaceBindings(Object3DData &data,const Object3DData &reference,
    std::span<const SurfaceBinding> bindings,std::span<std::uint8_t> moved={});
IIACTOROBJECT_EXPORT void applyMeshSurfaceBindingsWithBases(Object3DData &data,const Object3DData &reference,
    std::span<const SurfaceBinding> bindings,std::span<const std::optional<SurfaceBindingBasis>> bases,
    std::span<std::uint8_t> moved={});
// Exact inverse for absolute snapshot import. All source frames are read from the final data.
IIACTOROBJECT_EXPORT void unapplyMeshSurfaceBindings(Object3DData &data,const Object3DData &reference,
    std::span<const SurfaceBinding> bindings);
IIACTOROBJECT_EXPORT void unapplyMeshSurfaceBindingsWithBases(Object3DData &data,const Object3DData &reference,
    std::span<const SurfaceBinding> bindings,std::span<const std::optional<SurfaceBindingBasis>> bases);
} // namespace iiActorObject
