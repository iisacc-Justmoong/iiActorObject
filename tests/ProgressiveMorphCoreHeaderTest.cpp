#include <iiActorObject/Object3DProgressiveMorphCore.h>
#include <type_traits>

static_assert(std::is_default_constructible_v<iiActorObject::ProgressiveMorphProgram>);

int progressiveCoreHeaderProbe()
{
    return iiActorObject::object3DProgressiveControlCurveName("Smile") ==
                   "iiActorObject/progressive/Smile"
               ? 0 : 1;
}
