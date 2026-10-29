#pragma once
#include <iiActorObject/Animation.h>
#include "Object3DInterchange.h"
#include "../third_party/ufbx/ufbx.h"
#include <map>

namespace iiActorObject::object3d_detail
{
struct FbxImportedNode final
{
    const ufbx_node *node;
    const ufbx_node *parent;
    BoneIndex bone;
};
struct FbxImportedClip final
{
    std::string name;
    double sourceStartSeconds, duration;
    std::vector<BoneTrack> tracks;
    std::vector<AnimationCurve> curves;
    std::vector<AnimationMorphBinding> morphBindings;
};
struct FbxImportedMorph final
{
    std::string name;
    std::size_t mesh,target;
    std::vector<const ufbx_blend_channel *> channels;
};
using FbxImportedMorphs = std::vector<FbxImportedMorph>;
struct FbxImportedProgressiveControl final
{
    std::string name;
    std::vector<const ufbx_blend_channel *> channels;
};
std::vector<FbxImportedClip> planFbxImportedAnimations(const ufbx_scene &scene,
    std::span<const FbxImportedNode> nodes, const FbxImportedMorphs &morphs,
    std::span<const FbxImportedProgressiveControl> progressive,
    const FbxAnimationImportOptions &options, std::vector<std::string> &warnings);
} // namespace iiActorObject::object3d_detail
