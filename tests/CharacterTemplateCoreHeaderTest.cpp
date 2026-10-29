#include <iiActorObject/NSCharacter/CharacterTemplateCore.h>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_constructible_v<iiActorObject::NSCharacter::CharacterTemplate,
                                      iiActorObject::Object3DTemplateHandle>);

int characterTemplateCoreHeaderProbe()
{
    try
    {
        iiActorObject::NSCharacter::CharacterTemplate value{iiActorObject::Object3DTemplateHandle{}};
        (void)value;
    }
    catch(const std::invalid_argument &){return 0;}
    return 1;
}
