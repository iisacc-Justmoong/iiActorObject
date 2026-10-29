#pragma once
#include "MeshData.h"
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace iiActorObject { struct Bone; struct Object3DMesh; }
namespace iiActorObject::object3d_detail
{
// A GLB primitive attribute carries the source point ID of each rendered
// vertex. This optional, bounded sidecar stores points that no primitive uses.
struct GlbUnusedPoint final
{
    std::uint32_t id=0;
    Vector3 position;
    std::vector<SkinWeight> weights;
};
struct GlbUnusedPointMorph final
{
    std::uint32_t point=0,target=0;
    Vector3 delta;
};
struct GlbPointIdentity final
{
    std::uint32_t pointCount=0,targetCount=0;
    std::vector<std::string> targetNames,boneNames;
    std::vector<GlbUnusedPoint> unused;
    std::vector<GlbUnusedPointMorph> morphs;
    std::vector<std::uint32_t> emptyWeights;
};
[[nodiscard]] std::vector<std::uint8_t> encodeGlbPointIdentity(const Object3DMesh &mesh,std::span<const Bone> bones);
[[nodiscard]] GlbPointIdentity decodeGlbPointIdentity(std::span<const std::uint8_t> bytes,
    std::size_t maximumPoints,std::size_t maximumTargets,std::size_t maximumMorphs,
    std::size_t maximumInfluences,std::size_t maximumBones);
} // namespace iiActorObject::object3d_detail
