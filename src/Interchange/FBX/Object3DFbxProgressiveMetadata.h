#pragma once
#include "Object3DFbxMetadata.h"

namespace iiActorObject::object3d_detail
{
struct FbxProgressiveMetadata final
{
    bool marker=false,grouped=false,sparse=false,ignoredGroup=false,ignoredSparse=false;
    std::uint32_t groupId=0,groupSize=0;
    std::vector<std::uint32_t> presence;
};
// Parses and validates metadata only; the import coordinator owns warnings,
// channel grouping, mesh construction and all engine resources.
[[nodiscard]] FbxProgressiveMetadata readFbxProgressiveMetadata(
    const ufbx_blend_channel &channel,bool restore,bool progressiveAvailable,std::size_t maximumMorphs);
} // namespace iiActorObject::object3d_detail
