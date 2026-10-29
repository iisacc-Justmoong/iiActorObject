#pragma once
#include "Object3DFbxProgressiveData.h"
#include "Object3DProgressiveMorphCore.h"

namespace iiActorObject::object3d_detail
{
// Validates binding/defaults and output budgets without serializing a file.
[[nodiscard]] ProgressiveFbxPlans buildProgressiveFbxPlans(
    const Object3DAsset &asset,Object3DRigSession &session,const ProgressiveMorphProgram &program);
} // namespace iiActorObject::object3d_detail
