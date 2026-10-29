#include <iiActorObject/Object3DProgressiveMorphInterchange.h>
#include <type_traits>

static_assert(std::is_default_constructible_v<iiActorObject::Object3DProjectAnimation>);
static_assert(std::is_default_constructible_v<iiActorObject::FbxProgressiveTakesExportOptions>);

int progressiveInterchangeHeaderProbe()
{
    iiActorObject::FbxProgressiveMorphImport result;
    return result.rig.valid() ? 1 : 0;
}
