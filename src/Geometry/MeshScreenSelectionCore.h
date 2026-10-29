#pragma once
#include <iiActorObject/MeshEditing.h>
#include <array>
#include <cstdint>
#include <optional>
#include <variant>

namespace iiActorObject
{
struct MeshScreenPoint final {double x=0,y=0;};
struct MeshScreenRectangle final {MeshScreenPoint first,last;};
struct MeshScreenCircle final {MeshScreenPoint center;double radius=0;};
// The caller owns polygon coordinates for the duration of the selection call.
struct MeshScreenPolygon final {std::span<const MeshScreenPoint> points;};
using MeshScreenRegion=std::variant<MeshScreenRectangle,MeshScreenCircle,MeshScreenPolygon>;
struct MeshScreenViewport final
{
    double x=0,y=0,width=1,height=1;
    bool yDown=true;
};
enum class MeshScreenDepthRange {MinusOneToOne,ZeroToOne};
struct MeshScreenView final
{
    // Column-major homogeneous clip coordinates from the mesh's object space.
    std::array<double,16> clipFromObject{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    MeshScreenViewport viewport;
    MeshScreenDepthRange depthRange=MeshScreenDepthRange::MinusOneToOne;
    bool reversedDepth=false;
};
// The host supplies normalized [0,1] depth in the same viewport coordinate
// system; a missing sample is handled by the explicit missing-depth policy.
class IIACTOROBJECT_EXPORT MeshScreenDepthSampler
{
  public:
    virtual ~MeshScreenDepthSampler();
    [[nodiscard]] virtual std::optional<double> sample(MeshScreenPoint point) const=0;
};
enum class MeshScreenMissingDepth {Reject,Exclude,Include};
// Optional positions are in canonical source-point order (render order for a
// mesh without topology). They may describe a deformed pose.
struct MeshScreenMeshPositions final
{
    std::size_t mesh=0;
    std::span<const Vector3> positions;
    // Empty means all positions are available; otherwise each byte is 0 or 1.
    std::span<const std::uint8_t> available;
};
struct MeshScreenPositionSnapshot final
{
    std::vector<Vector3> positions;
    std::vector<std::uint8_t> available;
};
struct MeshScreenCaptureOptions final
{
    double seamTolerance=1e-8; // Object-space position distance.
    std::size_t maximumPoints=4'000'000,maximumRenderVertices=8'000'000;
};
// Resolves a deformed render snapshot back to original point IDs. Divergent
// render copies of one source point are rejected; points with no copy are marked unavailable.
[[nodiscard]] IIACTOROBJECT_EXPORT MeshScreenPositionSnapshot captureMeshScreenPositions(
    const Object3DMesh &mesh,std::span<const DeformedVertex> vertices,const MeshScreenCaptureOptions &options={});
struct MeshScreenSelectionOptions final
{
    std::span<const std::size_t> meshes; // Empty selects every mesh.
    std::span<const MeshScreenMeshPositions> positions;
    const MeshScreenDepthSampler *depthSampler=nullptr; // Null selects through surfaces.
    MeshScreenMissingDepth missingDepth=MeshScreenMissingDepth::Reject;
    bool includeUnusedPoints=false;
    double edgeTolerance=1e-8,depthTolerance=1e-5;
    std::size_t maximumPoints=4'000'000,maximumTriangleIndices=24'000'000;
    std::size_t maximumPolygonEdgeTests=128'000'000;
};
struct MeshScreenSelectionResult final
{
    // Sorted by mesh; each row is source-point order and may be all zero.
    std::vector<MeshPointMask> masks;
    std::size_t selectedPoints=0,skippedUnusedPoints=0,unavailablePoints=0,clippedPoints=0,outsideRegionPoints=0;
    std::size_t depthQueries=0,occludedPoints=0,missingDepthPoints=0,polygonEdgeTests=0;
};
// Pure value operation over caller-owned data.
[[nodiscard]] IIACTOROBJECT_EXPORT MeshScreenSelectionResult selectMeshPointsOnScreen(
    const Object3DData &data,const MeshScreenView &view,const MeshScreenRegion &region,
    const MeshScreenSelectionOptions &options={});
} // namespace iiActorObject
