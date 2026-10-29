#include <iiActorObject/NSCharacter/MakeHumanTemplate.h>
#include <type_traits>
#include <utility>

static_assert(std::is_default_constructible_v<iiActorObject::NSCharacter::MakeHumanTemplateOptions>);
static_assert(std::is_same_v<decltype(iiActorObject::NSCharacter::loadMakeHumanTemplate(
    std::declval<const iiActorObject::Object3DReference &>(),std::declval<const std::filesystem::path &>())),
    iiActorObject::NSCharacter::CharacterTemplate>);

int characterTemplateCoreHeaderProbe();
int humanAccessoriesHeaderProbe();
int main(){return characterTemplateCoreHeaderProbe()||humanAccessoriesHeaderProbe();}
