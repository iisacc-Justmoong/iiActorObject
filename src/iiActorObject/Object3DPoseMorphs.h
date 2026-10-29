#pragma once
#include <iiActorObject/Object3DPoseRig.h>
#include <iiActorObject/Object3DPoseSnapshot.h>

namespace iiActorObject
{
struct PoseMorphBakeOptions final
{
    bool replaceExisting=false;
    bool normals=true;
    double epsilon=1e-12;
    std::size_t maximumDeltas=16'000'000;
};
struct PoseMorphBakeResult final {std::size_t meshes=0,morphs=0,positionDeltas=0,normalDeltas=0;};
// An explicit snapshot bake after shape/rest/skin editing. Does not modify its program.
[[nodiscard]] IIACTOROBJECT_EXPORT PoseMorphBakeResult bakeObject3DPoseMorphs(Object3DData &data,
    const Object3DPoseProgram &program,std::span<const BoneIndex> jointMapping={},const PoseMorphBakeOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DHandle createObject3DPoseMorphs(const Object3DHandle &source,
    const Object3DPoseProgram &program,std::span<const BoneIndex> jointMapping={},const PoseMorphBakeOptions &options={});
// Sample the live program over the source's initial reference pose into a separate
// reference-bind exchange asset. This avoids cross terms from two successive bakes.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DHandle createReferenceObject3DPoseMorphs(const Object3DHandle &source,
    const Object3DPoseProgram &program,std::span<const BoneIndex> jointMapping={},const PoseMorphBakeOptions &options={},
    const Object3DPoseSnapshotOptions &snapshotOptions={});
} // namespace iiActorObject
