#include <iiActorObject/MeshPointMaskValidation.h>
#include <type_traits>

using MaskValidator=void (*)(const iiActorObject::Object3DData &,std::span<const iiActorObject::MeshPointMask>);
static_assert(std::is_same_v<decltype(&iiActorObject::validateMeshPointMasks),MaskValidator>);
