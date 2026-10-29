#include <iiActorObject/BoneIndex.h>
#include <limits>
#include <type_traits>

static_assert(std::is_same_v<iiActorObject::BoneIndex,std::size_t>);
static_assert(iiActorObject::NoBone == std::numeric_limits<std::size_t>::max());

int boneIndexHeaderProbe(){return 0;}
