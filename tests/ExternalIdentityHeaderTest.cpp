#include <iiActorObject/Object3DExternalIdentity.h>
#include <type_traits>

static_assert(std::is_default_constructible_v<iiActorObject::Object3DExternalIdentity>);
static_assert(std::is_default_constructible_v<iiActorObject::ExternalMeshIdentity>);
