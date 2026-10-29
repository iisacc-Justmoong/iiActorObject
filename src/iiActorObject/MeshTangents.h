#pragma once
#include <iiActorObject/MeshTopology.h>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace iiActorObject
{
struct MeshCornerTangent final
{
    Vector3 direction;
    // Bitangent = handedness * cross(normal, direction).
    double handedness=1;
};
struct MeshTangentOptions final
{
    std::size_t maxCorners=8'000'000;
    // Compute in a coordinate system where V is 1 - the supplied V.
    bool flipTextureV=false;
};

// One unindexed result per triangle corner, in exactly the order of triangles.
// The source vertices/indices/UVs are borrowed and never modified.
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<MeshCornerTangent> calculateMeshCornerTangents(
    std::span<const DeformedVertex> vertices, std::span<const std::uint32_t> triangles,
    std::span<const TextureCoordinate> uv, MeshTangentOptions options={});
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<MeshCornerTangent> calculateMeshCornerTangents(
    const SkeletalMeshLod &mesh, std::span<const TextureCoordinate> uv, MeshTangentOptions options={});
} // namespace iiActorObject
