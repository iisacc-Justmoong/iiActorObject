#pragma once
#include "Object3DEditorRig.h"

namespace iiActorObject::object3d_detail
{
void validateEditedJointIdentity(const Object3DAsset &previous,const Object3DAsset &next,EditedRigJointIdentity policy);
}
