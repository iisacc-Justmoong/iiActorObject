#pragma once
#include "Object3DInterchange.h"
#include "Object3DFbxAnimationPlan.h"

namespace iiActorObject::object3d_detail
{
FbxAnimationPlan planFbxAnimation(const Object3DAsset &, const AnimationClip &,
    const FbxAnimationExportOptions &, const FbxMorphChannels &,
    const FbxProgressiveChannels &,bool progressiveMode);
} // namespace iiActorObject::object3d_detail
