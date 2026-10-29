#pragma once
#include <iiActorObject/Object3DProject.h>

namespace iiActorObject
{
// Copies an animation onto a new asset snapshot only if bone identities and
// bind transforms match and all bound mesh/morph identities remain unambiguous.
// Neither asset nor the original clip is modified.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DProjectAnimation rebindObject3DProjectAnimation(
    const Object3DProjectAnimation &animation,const Object3DHandle &target);
} // namespace iiActorObject
