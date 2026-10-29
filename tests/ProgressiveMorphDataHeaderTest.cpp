#include <iiActorObject/Object3DProgressiveMorphData.h>
#include <type_traits>

static_assert(std::is_default_constructible_v<iiActorObject::ProgressiveMorphProgram>);
int progressiveDataHeaderProbe()
{
    iiActorObject::ProgressiveMorphProgram program;
    program.channels.push_back({"Smile",.25,{{"Half",.5},{"Full",1}}});
    return program.channels[0].keys.size()==2&&program.channels[0].defaultValue==.25?0:1;
}
