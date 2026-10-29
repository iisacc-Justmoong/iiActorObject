#include <iiActorObject/SkeletalMeshCore.h>
#include <type_traits>

static_assert(std::is_class_v<iiActorObject::SkeletalMesh>);
int skeletalMeshCoreHeaderProbe()
{
    auto deform=&iiActorObject::SkeletalMesh::deform;
    return deform ? 0 : 1;
}
