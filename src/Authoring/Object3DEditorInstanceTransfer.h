#pragma once
#include <iiActorObject/Object3DInstanceTransferCore.h>
#include <iiActorObject/Object3DEditorRig.h>

namespace iiActorObject
{
// Prepares the actor first and then atomically synchronizes the rig set. The
// source must be the set's last bound object. A failure leaves old rigs intact.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DInstanceTransferResult transferObject3DEditorInstance(
    const Object3DHandle &source,const iiCharacter &actor,Object3DEditorRigSet &rigs,
    const Object3DInstanceTransferOptions &options={});
} // namespace iiActorObject
