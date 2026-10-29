#pragma once
#include "Object3DInterchange.h"
#include <array>
#include <map>

namespace iiActorObject::object3d_detail
{
struct FbxTransformSample final
{
    std::int64_t tick;
    std::array<float, 3> position, rotation, scale;
};
struct FbxTransformTrack final
{
    BoneIndex bone;
    std::vector<FbxTransformSample> samples;
};
struct FbxScalarSample final
{
    std::int64_t tick;
    float value;
};
struct FbxMorphTrack final
{
    std::vector<std::int64_t> channels;
    std::vector<FbxScalarSample> samples;
};
struct FbxAnimationPlan final
{
    std::int64_t duration;
    std::vector<FbxTransformTrack> transforms;
    std::vector<FbxMorphTrack> morphs;
    std::size_t sampleCount=0;
};
using FbxMorphChannels = std::map<std::pair<std::size_t,std::size_t>,std::vector<std::int64_t>>;
using FbxProgressiveChannels = std::map<std::string,std::vector<std::int64_t>,std::less<>>;
FbxAnimationPlan planFbxAnimation(const Object3DAsset &, const AnimationClip &,
    const FbxAnimationExportOptions &, const FbxMorphChannels &,
    const FbxProgressiveChannels &,bool progressiveMode);
} // namespace iiActorObject::object3d_detail
