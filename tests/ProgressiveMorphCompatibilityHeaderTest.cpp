#include <iiActorObject/Object3DProgressiveMorph.h>
#include <type_traits>

// The original aggregate still supplies the project declarations to callers.
static_assert(std::is_default_constructible_v<iiActorObject::Object3DProject>);
static_assert(std::is_default_constructible_v<iiActorObject::Object3DProjectOptions>);

int progressiveCoreHeaderProbe();
int progressiveInterchangeHeaderProbe();
int progressiveDataHeaderProbe();
int main(){return progressiveDataHeaderProbe() || progressiveCoreHeaderProbe() || progressiveInterchangeHeaderProbe();}
