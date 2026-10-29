#pragma once
#include "MeshTopologyCore.h"
#include "Object3DFbxBinary.h"
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace iiActorObject::object3d_detail
{
struct FbxMorphPayload final
{
    std::vector<std::int32_t> indices;
    std::vector<double> deltas,normalDeltas;
    fbx_binary::Bytes normalMetadata;
    bool exactNormals=true,explicitNormals=false;
};
// Projects a borrowed morph and optional source layout into owned FBX arrays.
[[nodiscard]] FbxMorphPayload encodeFbxMorph(const TopologyMorph &morph,const MeshTopology *source,
    std::size_t pointCount,bool allowInexactNormals,bool progressiveShape,std::size_t &progressiveWork);
[[nodiscard]] fbx_binary::Node makeFbxMorphShape(const FbxMorphPayload &payload,std::string_view name,std::int64_t id);
} // namespace iiActorObject::object3d_detail
