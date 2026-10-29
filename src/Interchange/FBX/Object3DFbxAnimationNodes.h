#pragma once
#include "Object3DFbxAnimationPlan.h"
#include "Object3DFbxBinary.h"
#include <span>
#include <string_view>

namespace iiActorObject::object3d_detail
{
// Append the encoded nodes for one sampled take; the caller owns the scene and IDs.
void appendFbxAnimationNodes(const FbxAnimationPlan &plan,std::string_view name,
    std::size_t takeIndex,std::size_t takeCount,double startSeconds,std::span<const std::int64_t> bones,
    std::int64_t &next,fbx_binary::Node &objects,fbx_binary::Node &connections,fbx_binary::Node &takes);
} // namespace iiActorObject::object3d_detail
