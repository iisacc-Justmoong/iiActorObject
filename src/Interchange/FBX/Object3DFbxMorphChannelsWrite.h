#pragma once
#include "Object3DFbxAnimationPlan.h"
#include "Object3DFbxBinary.h"
#include "Object3DFbxProgressiveData.h"
#include "Object3DMesh.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace iiActorObject::object3d_detail
{
struct FbxMorphRecords final
{
    std::vector<TopologyMorph> morphs;
    // Empty for separate records; otherwise one source-record count per morph.
    std::vector<std::size_t> projectedRecordCounts;
};
[[nodiscard]] FbxMorphRecords collectFbxMorphRecords(const Object3DMesh &mesh,bool groupLogicalChannels);

using FbxProgressiveCurveNamer=std::string (*)(std::string_view);
struct FbxMorphChannelWriteInput final
{
    const Object3DMesh &mesh;
    const FbxMorphRecords &records;
    std::size_t meshIndex;
    std::int64_t geometry;
    const ProgressiveFbxPlan *progressive=nullptr;
    bool includeAnimation=false;
    bool allowMorphNormalMetadata=false;
    FbxProgressiveCurveNamer curveName=nullptr;
};
struct FbxMorphChannelWriteState final
{
    std::int64_t &nextId;
    fbx_binary::Node &objects;
    fbx_binary::Node &connections;
    FbxMorphChannels &morphChannels;
    FbxProgressiveChannels &progressiveChannels;
    std::size_t &progressiveShapeWork;
};
// Inputs are borrowed; progressive channels and used topology layouts must be valid.
// Nodes, ID counters, channel maps and work may be
// partially updated on failure; the owner must discard this staged state.
void writeFbxMorphChannels(const FbxMorphChannelWriteInput &input,FbxMorphChannelWriteState &state);
}
