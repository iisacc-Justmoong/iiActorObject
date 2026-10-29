#include <iiActorObject/Object3DData.h>
#include <type_traits>

static_assert(std::is_default_constructible_v<iiActorObject::Object3DData>);
int object3DDataHeaderProbe()
{
    const iiActorObject::Object3DData data;
    return data.meshes.empty()&&data.materials.size()==1 ? 0 : 1;
}
