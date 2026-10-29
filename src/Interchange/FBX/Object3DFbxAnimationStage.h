#pragma once
#include <iiActorObject/Animation.h>
#include "Object3DMesh.h"

namespace iiActorObject::object3d_detail
{
struct FbxImportedClip final
{
    std::string name;
    double sourceStartSeconds,duration;
    std::vector<BoneTrack> tracks;
    std::vector<AnimationCurve> curves;
    std::vector<AnimationMorphBinding> morphBindings;
};
struct FbxStagedAnimation final
{
    std::string name;
    double sourceStartSeconds;
    std::shared_ptr<const AnimationClip> clip;
    std::vector<AnimationMorphBinding> morphBindings;
};
// Validate against a temporary skeleton before an engine asset is created.
void validateFbxAnimationClips(std::span<const Bone> bones,std::span<const FbxImportedClip> clips);
// Return complete value results, borrowing the asset skeleton and meshes; never
// publish an engine object or mutate a caller-visible output on failure.
[[nodiscard]] std::vector<FbxStagedAnimation> stageFbxAnimationClips(
    std::shared_ptr<const Skeleton> skeleton,std::span<const Object3DMesh> meshes,
    std::vector<FbxImportedClip> clips);
}
