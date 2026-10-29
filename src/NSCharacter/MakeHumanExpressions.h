#pragma once
#include "MakeHumanResourceReader.h"
#include <iiActorObject/Object3DPoseRig.h>
#include <iiActorObject/Object3DAsset.h>

namespace iiActorObject::NSCharacter::human_detail
{
[[nodiscard]] Object3DPoseProgram expressionAssets(const Object3DData &data,Files &files,double scale);
} // namespace iiActorObject::NSCharacter::human_detail
