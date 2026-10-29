#pragma once
#include <iiActorObject/BoneIndex.h>
#include <iiActorObject/Transform.h>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace iiActorObject
{
struct SkinWeight
{
    BoneIndex bone = NoBone;
    double weight = 0;
};
IIACTOROBJECT_EXPORT void normalizeSkinWeights(std::span<SkinWeight> weights, std::size_t boneCount);
struct SkinVertex
{
    Vector3 position;
    Vector3 normal{0, 0, 1};
    std::vector<SkinWeight> weights;
};
struct MorphDelta
{
    std::size_t vertex = 0;
    Vector3 position;
    Vector3 normal;
};
struct MorphTarget
{
    std::string name;
    std::vector<MorphDelta> deltas;
};
enum class SkinningMethod : std::uint8_t { Linear, DualQuaternion, Blended };
// Blended values are in source-point order for topology, otherwise render-vertex order.
struct SkinningProfile final
{
    SkinningMethod method = SkinningMethod::Linear;
    std::vector<double> blendWeights;
    bool operator==(const SkinningProfile &) const = default;
};
IIACTOROBJECT_EXPORT void validateSkinningProfile(const SkinningProfile &profile, std::size_t pointCount);
struct SkeletalMeshLod
{
    std::vector<SkinVertex> vertices;
    std::vector<std::uint32_t> triangles;
    std::vector<MorphTarget> morphTargets;
    SkinningProfile skinning;
};
struct DeformedVertex
{
    Vector3 position;
    Vector3 normal;
};

} // namespace iiActorObject
