#pragma once
#include "Object3DAsset.h"
#include "Object3DJointIdentity.h"

namespace iiActorObject::object3d_detail
{
void validateEditedJointIdentity(const Object3DAsset &previous,const Object3DAsset &next,EditedRigJointIdentity policy);
}
