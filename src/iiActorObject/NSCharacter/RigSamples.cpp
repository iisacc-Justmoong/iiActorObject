#include "RigSamples.h"
#include <stdexcept>

namespace iiActorObject::NSCharacter
{
std::shared_ptr<const CharacterAsset> createCharacterRigMorphSamples(const CharacterAsset &source,
    const Object3DRigHandle &rig,const RigSampleInput &baseline,std::span<const RigMorphRequest> samples,
    const RigMorphSampleOptions &options)
{
    return createCharacterRigMorphSamples(source,rig,baseline,samples,options,Object3DSnapshotSkinning::Preserve);
}
std::shared_ptr<const CharacterAsset> createCharacterRigMorphSamples(const CharacterAsset &source,
    const Object3DRigHandle &rig,const RigSampleInput &baseline,std::span<const RigMorphRequest> samples,
    const RigMorphSampleOptions &options,Object3DSnapshotSkinning skinning)
{
    if(source.object()!=rig.get().object())throw std::invalid_argument("Character and rig must reference the same core object");
    return std::make_shared<CharacterAsset>(createObject3DRigMorphSamples(rig,baseline,samples,options,skinning));
}
}
