#pragma once
#include <iiActorObject/Object3DProject.h>

namespace iiActorObject::object3d_detail
{
void validateProjectAnimations(std::span<const Object3DProjectAnimation> animations,const Object3DHandle &object);
} // namespace iiActorObject::object3d_detail
