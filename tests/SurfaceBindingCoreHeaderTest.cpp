#include <iiActorObject/SurfaceBindingCore.h>
#include <type_traits>

static_assert(std::is_default_constructible_v<iiActorObject::SurfaceBinding>);
int surfaceBindingCoreHeaderProbe()
{
    using Function = std::vector<iiActorObject::Vector3> (*)(
        const iiActorObject::Object3DData &, const iiActorObject::SurfaceBinding &);
    return static_cast<Function>(&iiActorObject::evaluateMeshSurfaceBinding) ? 0 : 1;
}
