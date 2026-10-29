#pragma once
#include <iiActorObject/SkeletalMesh.h>
#include <optional>

namespace iiActorObject
{
inline constexpr std::uint32_t NoTopologyIndex = std::numeric_limits<std::uint32_t>::max();
struct TextureCoordinate final
{
    double u = 0, v = 0;
    constexpr bool operator==(const TextureCoordinate &) const = default;
};
struct TextureCoordinateSet final
{
    std::string name;
    std::vector<TextureCoordinate> coordinates;
    bool operator==(const TextureCoordinateSet &) const = default;
};
struct TopologyTextureCoordinateSet final
{
    std::string name;
    std::vector<TextureCoordinate> coordinates;
    // One source coordinate ID per layout; unused or hidden layouts may use NoTopologyIndex.
    std::vector<std::uint32_t> layoutIndices;
    bool operator==(const TopologyTextureCoordinateSet &) const = default;
};
struct TopologyControlPoint final
{
    Vector3 position;
    std::vector<SkinWeight> weights;
};
struct TopologyVertexLayout final
{
    std::uint32_t position = 0, textureCoordinate = NoTopologyIndex, normal = NoTopologyIndex;
    constexpr bool operator==(const TopologyVertexLayout &) const = default;
};
struct MeshTopologyDefinition final
{
    std::string sourceFormat;
    std::uint32_t sourceElement = NoTopologyIndex;
    std::uint32_t positionCount = 0, textureCoordinateCount = 0, normalCount = 0;
    std::vector<TopologyVertexLayout> layouts;
    std::vector<std::vector<std::uint32_t>> polygons;
    // Optional source face labels. Hidden faces retain their original IDs and anchors.
    std::vector<std::string> polygonGroups;
    std::vector<std::uint32_t> hiddenPolygons;
    std::vector<std::uint32_t> renderToLayout;
    bool reverseWinding = false;
    std::vector<std::uint32_t> polygonMaterials;
    bool operator==(const MeshTopologyDefinition &) const = default;
};
// Immutable source IDs/connectivity and a stable triangulation shared by snapshots.
class IIACTOROBJECT_EXPORT MeshTopologyLayout final
{
  public:
    [[nodiscard]] static std::shared_ptr<const MeshTopologyLayout> create(
        MeshTopologyDefinition definition, std::span<const TopologyControlPoint> points);
    // Restores an already-authored triangulation without deriving new diagonals from edited points.
    // Validates oriented polygon boundaries, internal edges and noncrossing source-corner diagonals.
    [[nodiscard]] static std::shared_ptr<const MeshTopologyLayout> restore(
        MeshTopologyDefinition definition,std::span<const TopologyControlPoint> points,
        std::vector<std::uint32_t> triangles,std::vector<std::uint32_t> trianglePolygons);
    [[nodiscard]] const MeshTopologyDefinition &definition() const noexcept;
    [[nodiscard]] std::span<const std::uint32_t> triangles() const noexcept;
    [[nodiscard]] std::span<const std::uint32_t> trianglePolygons() const noexcept;
  private:
    MeshTopologyLayout(MeshTopologyDefinition definition,std::vector<std::uint32_t> triangles,
        std::vector<std::uint32_t> trianglePolygons);
    const MeshTopologyDefinition definition_;
    const std::vector<std::uint32_t> triangles_;
    const std::vector<std::uint32_t> trianglePolygons_;
};
struct TopologyPointDelta final
{
    std::uint32_t point = 0;
    Vector3 position;
    bool operator==(const TopologyPointDelta &) const=default;
};
struct TopologyNormalDelta final
{
    std::uint32_t layout=0;
    Vector3 normal;
    bool operator==(const TopologyNormalDelta &) const=default;
};
struct TopologyMorph final
{
    std::string name;
    std::vector<TopologyPointDelta> deltas;
    std::uint32_t sourceTarget = NoTopologyIndex;
    // nullopt derives normals from geometry; an empty explicit list means no normal change.
    std::optional<std::vector<TopologyNormalDelta>> normalDeltas = std::nullopt;
    bool operator==(const TopologyMorph &) const=default;
};
// Authored FBX attributes use visible polygon corners, in exported winding order.
// Empty vectors mean that the corresponding source attribute was absent.
struct TopologyTangentSourceLayer final
{
    std::size_t uvSet = 0;
    std::vector<Vector3> tangents, binormals;
    std::vector<double> tangentW, binormalW;
};
// Immutable original basis and the geometric state on which it depends. Shared
// by editor history; an altered state cannot silently reuse the original basis.
struct TopologyTangentSource final
{
    MeshTopologyDefinition definition;
    std::vector<std::uint32_t> triangles, trianglePolygons;
    std::vector<Vector3> positions, normals;
    std::vector<TextureCoordinate> textureCoordinates;
    std::string primaryUvName;
    std::vector<TopologyTextureCoordinateSet> additionalUvSets;
    std::vector<TopologyTangentSourceLayer> layers;
};
// glTF tangent values are indexed by the authoring layout, including unused
// layouts. They retain glTF's UV orientation and handedness convention.
struct TopologyGlbTangent final
{
    std::uint32_t layout=0;
    Vector3 direction;
    double handedness=1;
};
struct TopologyGlbTangentDelta final
{
    std::uint32_t layout=0;
    Vector3 direction;
};
struct TopologyGlbTangentBinding final
{
    std::size_t material=0;
    bool normalMapped=false;
    std::string normalUvSet;
};
// Shared by undo history; the saved authoring state detects edits that make an
// authored basis or morph tangent no longer applicable.
struct TopologyGlbTangentSource final
{
    MeshTopologyDefinition definition;
    std::vector<std::uint32_t> triangles,trianglePolygons;
    std::vector<Vector3> positions,normals;
    std::vector<TextureCoordinate> textureCoordinates;
    std::string primaryUvName;
    std::vector<TopologyTextureCoordinateSet> additionalUvSets;
    std::vector<TopologyMorph> morphTargets;
    std::vector<TopologyGlbTangent> tangents;
    std::vector<std::vector<TopologyGlbTangentDelta>> morphTangents;
    std::vector<TopologyGlbTangentBinding> bindings;
};
struct MeshTopology final
{
    std::shared_ptr<const MeshTopologyLayout> layout;
    std::vector<TopologyControlPoint> points;
    std::vector<Vector3> normals;
    std::vector<TextureCoordinate> textureCoordinates;
    std::vector<TopologyMorph> morphTargets;
    std::size_t maximumRenderMorphDeltas = 16'000'000;
    std::string primaryUvName;
    std::vector<TopologyTextureCoordinateSet> additionalUvSets;
    std::shared_ptr<const TopologyTangentSource> sourceTangents;
    SkinningProfile skinning;
    std::shared_ptr<const TopologyGlbTangentSource> sourceGlbTangents;
};
struct TopologyPointMove final { std::uint32_t point = 0; Vector3 displacement; };
struct TopologyPointWeights final { std::uint32_t point = 0; std::vector<SkinWeight> weights; };
struct TopologyUvEdit final { std::uint32_t index = 0; TextureCoordinate value; };
struct TopologyRenderData final
{
    SkeletalMeshLod geometry;
    std::vector<TextureCoordinate> textureCoordinates;
    std::vector<std::size_t> triangleMaterials;
    std::string primaryUvName;
    std::vector<TextureCoordinateSet> additionalUvSets;
};
IIACTOROBJECT_EXPORT void normalizeMeshTopology(MeshTopology &topology, std::size_t boneCount);
// False after topology, geometry, normal or UV edits; undo may make it true again.
[[nodiscard]] IIACTOROBJECT_EXPORT bool topologySourceTangentsValid(const MeshTopology &topology);
[[nodiscard]] IIACTOROBJECT_EXPORT bool topologyGlbTangentsValid(const MeshTopology &topology);
[[nodiscard]] IIACTOROBJECT_EXPORT TopologyRenderData renderMeshTopology(const MeshTopology &topology);
IIACTOROBJECT_EXPORT void recomputeTopologyNormals(MeshTopology &topology);
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<TopologyNormalDelta> topologyMorphNormalDeltas(
    const MeshTopology &topology,const TopologyMorph &morph);
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<std::vector<std::size_t>> topologyAdjacency(
    const MeshTopologyLayout &layout);
// Resolves a render edit to source points; conflicting edits across a seam are rejected.
IIACTOROBJECT_EXPORT void reconcileTopologyRender(MeshTopology &topology,
    const SkeletalMeshLod &geometry, std::span<const TextureCoordinate> uv, std::size_t boneCount);
} // namespace iiActorObject
