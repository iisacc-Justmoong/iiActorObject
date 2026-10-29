#include <iiActorObject/Object3DMesh.h>
#include <type_traits>

static_assert(std::is_default_constructible_v<iiActorObject::Object3DMesh>);
int object3DMeshHeaderProbe()
{
    iiActorObject::Object3DMesh mesh;
    mesh.name="Reusable mesh";
    mesh.morphDefaults.push_back({"Smile",.25});
    return mesh.name=="Reusable mesh"&&mesh.morphDefaults[0].value==.25&&mesh.geometry.vertices.empty()?0:1;
}
