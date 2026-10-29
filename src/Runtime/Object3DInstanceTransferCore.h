#pragma once
#include <iiActorObject/Object3DEngine.h>
#include <iiActorObject/Object3DJointIdentity.h>

namespace iiActorObject
{
enum class Object3DInstancePoseTransfer : std::uint8_t { BindRelative, PreserveWorld };
struct Object3DInstanceTransferOptions final
{
    EditedRigJointIdentity jointIdentity=EditedRigJointIdentity::RequireEvidence;
    Object3DInstancePoseTransfer pose=Object3DInstancePoseTransfer::BindRelative;
    bool discardAnimation=false;
    bool discardMissingMorphs=false;
    bool discardExtraComponents=false;
    bool discardIncompatibleSkinOverrides=false;
    bool discardIncompatibleDeformationBind=false;
    double matrixTolerance=1e-9;
};
struct Object3DInstanceTransferReport final
{
    std::size_t matchedMeshes=0,addedMeshes=0,removedMeshes=0,suppressedMeshes=0;
    std::size_t copiedMorphs=0,discardedMorphs=0,discardedComponents=0,reattachedComponents=0;
    std::size_t copiedSkinOverrides=0,discardedSkinOverrides=0;
    bool copiedAnimation=false,discardedAnimation=false,discardedDeformationBind=false,rigsRebound=false;
    bool omittedAnimationError=false;
};
struct Object3DInstanceTransferResult final
{
    std::unique_ptr<iiCharacter> actor;
    Object3DInstanceTransferReport report;
};
// Creates a new target instance without changing the source actor or either asset.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DInstanceTransferResult transferObject3DInstance(
    const Object3DHandle &source,const iiCharacter &actor,const Object3DHandle &target,
    const Object3DInstanceTransferOptions &options={});
} // namespace iiActorObject
