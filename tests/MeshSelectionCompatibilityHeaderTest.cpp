#include <iiActorObject/MeshMaskProjection.h>
#include <iiActorObject/MeshScreenSelection.h>
#include <type_traits>

static_assert(std::is_default_constructible_v<iiActorObject::MeshScreenSelectionOptions>);
static_assert(std::is_default_constructible_v<iiActorObject::MeshMaskProjectionOptions>);
int meshSelectionCompatibilityHeaderProbe()
{
    return iiActorObject::MeshScreenSelectionResult{}.masks.empty() ? 0 : 1;
}
