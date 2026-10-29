#include <iiActorObject/FbxImageResolver.h>
#include <type_traits>

static_assert(std::is_abstract_v<iiActorObject::FbxImageResolver>);
static_assert(std::is_same_v<decltype(iiActorObject::FbxImageRequest{}.remainingBytes),std::size_t>);
int main() { return 0; }
