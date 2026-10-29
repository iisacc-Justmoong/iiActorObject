#include <iiActorObject/SkeletalMeshComponent.h>
#include <type_traits>

static_assert(std::is_base_of_v<iiActorObject::SceneComponent,iiActorObject::SkeletalMeshComponent>);
int skeletalMeshComponentHeaderProbe()
{
    auto pose=&iiActorObject::SkeletalMeshComponent::boundPose;
    return pose ? 0 : 1;
}
