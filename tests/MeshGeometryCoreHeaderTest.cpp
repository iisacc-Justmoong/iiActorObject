#include <iiActorObject/MeshGeometryCore.h>
#include <type_traits>

static_assert(std::is_default_constructible_v<iiActorObject::SkeletalMeshLod>);
int meshGeometryCoreHeaderProbe()
{
    auto *validate = &iiActorObject::validateMeshGeometry;
    return validate ? 0 : 1;
}
