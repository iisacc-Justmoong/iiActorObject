#pragma once
#include <iiActorObject/Object3DRig.h>
#include <iiActorObject/Object3DPoseSnapshot.h>

namespace iiActorObject
{
// Consume an existing evaluation without running or cloning its backend.
// Bake the pose and name-mapped blend shapes exactly as applyTo() would.
// Animated maps remain in the caller's unchanged evaluation for shader use.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DHandle createObject3DRigSnapshot(
    const Object3DRigHandle &rig,const Object3DRigEvaluation &evaluation,
    const Object3DPoseSnapshotOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DHandle createObject3DRigSnapshot(
    const Object3DRigHandle &rig,const Object3DRigEvaluation &evaluation,
    const Object3DPoseSnapshotOptions &options,Object3DSnapshotSkinning skinning);
}
