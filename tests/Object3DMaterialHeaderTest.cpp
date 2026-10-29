#include <iiActorObject/Object3DMaterial.h>
#include <type_traits>

static_assert(std::is_default_constructible_v<iiActorObject::Object3DMaterial>);
int object3DMaterialHeaderProbe()
{
    iiActorObject::Object3DMaterial material;
    if(iiActorObject::effectiveAlphaMode(material)!=iiActorObject::Object3DAlphaMode::Opaque)return 1;
    material.baseColor[3]=.5;
    if(iiActorObject::effectiveAlphaMode(material)!=iiActorObject::Object3DAlphaMode::Blend)return 2;
    material.surface.alphaMode=iiActorObject::Object3DAlphaMode::Mask;
    return iiActorObject::effectiveAlphaMode(material)==iiActorObject::Object3DAlphaMode::Mask?0:3;
}
