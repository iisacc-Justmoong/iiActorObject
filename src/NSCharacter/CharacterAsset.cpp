#include "CharacterAsset.h"
#include <stdexcept>
namespace iiActorObject::NSCharacter
{
CharacterAsset::CharacterAsset(Object3DHandle object) : object_(std::move(object))
{
    if (!object_.valid())
        throw std::invalid_argument("Character needs a live core 3D object reference");
}
bool CharacterAsset::valid() const noexcept
{
    return object_.valid();
}
const Object3DHandle &CharacterAsset::object() const noexcept
{
    return object_;
}
const CharacterData &CharacterAsset::data() const
{
    return object_.get().data();
}
const std::shared_ptr<const Skeleton> &CharacterAsset::skeleton() const
{
    return object_.get().skeleton();
}
std::span<const std::shared_ptr<const SkeletalMesh>> CharacterAsset::meshes() const
{
    return object_.get().meshes();
}
BoneIndex CharacterAsset::joint(std::string_view semantic) const
{
    return object_.get().joint(semantic);
}
std::unique_ptr<iiCharacter> CharacterAsset::instantiate(std::string name) const
{
    return object_.get().instantiate(std::move(name));
}
} // namespace iiActorObject::NSCharacter
