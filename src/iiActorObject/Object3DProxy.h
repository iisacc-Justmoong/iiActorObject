#pragma once
#include <iiActorObject/SurfaceBinding.h>

namespace iiActorObject
{
struct Object3DProxyDefinition final
{
    std::string name,baseMesh,objectFile,materialFile;
    std::vector<SurfaceControlPoint> points;
    std::array<std::optional<SurfaceBindingAxisScale>,3> offsetScales{};
    // Sorted, unique IDs on the source mesh which the asset marks as covered.
    std::vector<std::uint32_t> hiddenSourcePoints;
};
// Reads a bounded authored MHCLO mapping. It never opens referenced paths.
// One-ID and three-ID/three-weight/three-offset rows are supported; source
// coefficients remain unchanged and unitScale converts offsets/reference spans.
// A zero targetPoints value infers the row count for descriptor-first loading.
// delete_verts source IDs and inclusive ranges are retained, not applied.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DProxyDefinition parseObject3DProxy(
    std::string_view text,std::size_t sourcePoints,std::size_t targetPoints,double unitScale=1,
    std::size_t maximumBytes=16*1024*1024);
// Hide a visible polygon only when every source point is covered. Source IDs,
// hidden polygons and the remaining triangulation are preserved. Throws before
// changing the topology if IDs are invalid or no visible surface would remain.
[[nodiscard]] IIACTOROBJECT_EXPORT std::size_t hideObject3DProxyCoveredPolygons(
    MeshTopology &topology,std::span<const std::uint32_t> coveredPoints);
} // namespace iiActorObject
