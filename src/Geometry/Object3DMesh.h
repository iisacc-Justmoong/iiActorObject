#pragma once
#include <iiActorObject/MeshTopologyCore.h>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace iiActorObject
{
// A named morph's initial value; an omitted name has an initial value of zero.
struct MorphValue final
{
    std::string name;
    double value = 0;
    bool operator==(const MorphValue &) const = default;
};
struct Object3DMesh final
{
    std::string name;
    SkeletalMeshLod geometry;
    std::vector<TextureCoordinate> textureCoordinates;
    std::size_t material = 0;
    // When present, this is the authoring source of truth; geometry/UV are derived caches.
    std::optional<MeshTopology> topology;
    // Optional material ID per derived triangle; empty uses the mesh material.
    std::vector<std::size_t> triangleMaterials;
    // Derived from topology when present; otherwise one coordinate per render vertex.
    std::string primaryUvName;
    std::vector<TextureCoordinateSet> additionalUvSets;
    // Named defaults are owned by the asset, independently of each runtime instance.
    std::vector<MorphValue> morphDefaults;
};
} // namespace iiActorObject
