#include <iiActorObject/Object3DInstanceTransferCore.h>
#include <type_traits>

static_assert(std::is_default_constructible_v<iiActorObject::Object3DInstanceTransferOptions>);
int instanceTransferCoreHeaderProbe()
{
    auto *transfer = &iiActorObject::transferObject3DInstance;
    return transfer ? 0 : 1;
}
