#pragma once
#include <iiActorObject/NSCharacter/CharacterAsset.h>
#include <iiActorObject/Object3DRigSamples.h>

namespace iiActorObject::NSCharacter
{
// Returns a human view of the new core-owned sampled asset. The source view and
// rig must refer to the same object; neither engine nor geometry is stored here.
[[nodiscard]] IIACTOROBJECT_EXPORT std::shared_ptr<const CharacterAsset> createCharacterRigMorphSamples(
    const CharacterAsset &source,const Object3DRigHandle &rig,const RigSampleInput &baseline,
    std::span<const RigMorphRequest> samples,const RigMorphSampleOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT std::shared_ptr<const CharacterAsset> createCharacterRigMorphSamples(
    const CharacterAsset &source,const Object3DRigHandle &rig,const RigSampleInput &baseline,
    std::span<const RigMorphRequest> samples,const RigMorphSampleOptions &options,Object3DSnapshotSkinning skinning);
}
