#include <iiActorObject/NSCharacter/HumanAccessories.h>
#include <type_traits>

static_assert(std::is_default_constructible_v<iiActorObject::NSCharacter::HumanAccessorySelection>);

int humanAccessoriesHeaderProbe()
{
    iiActorObject::NSCharacter::HumanAccessorySelection selection;
    return selection.hideCoveredBody&&selection.componentIds.empty()?0:1;
}
