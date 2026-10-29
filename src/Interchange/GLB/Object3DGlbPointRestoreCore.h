#pragma once
#include "MeshTopologyCore.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace iiActorObject::object3d_detail
{
struct GlbPointIdentity;

struct GlbPointRestoreOptions final
{
    // Column-major 4x3 affine transform: X, Y, Z and translation columns.
    std::array<double,12> affine{1,0,0,0,1,0,0,0,1,0,0,0};
    std::span<const BoneIndex> sidecarToBone;
    BoneIndex rigidBone=NoBone;
    std::size_t maximumPoints=2'000'000;
    std::size_t maximumMorphDeltas=16'000'000;
    bool skinned=false;
};

struct GlbPointRestorePlan final
{
    std::vector<TopologyControlPoint> points;
    std::vector<std::vector<TopologyPointDelta>> morphs;
    std::vector<std::uint32_t> layoutPoints;
};

// Builds a new point/morph mapping without modifying its inputs. The caller
// validates the source document and publishes the complete plan on success.
[[nodiscard]] GlbPointRestorePlan planGlbPointRestore(const GlbPointIdentity &saved,
    const MeshTopology &topology,const MeshTopologyDefinition &definition,
    std::span<const std::uint32_t> ids,const GlbPointRestoreOptions &options);
}
