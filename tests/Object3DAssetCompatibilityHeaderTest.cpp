#include <iiActorObject/Object3DAsset.h>
#include <type_traits>

// The original asset header keeps both the data and runtime declarations.
static_assert(std::is_default_constructible_v<iiActorObject::Object3DData>);
static_assert(std::is_class_v<iiActorObject::iiCharacter>);
int object3DAssetCompatibilityHeaderProbe()
{
    return iiActorObject::Object3DKind::Automatic==iiActorObject::Object3DData{}.kind ? 0 : 1;
}
