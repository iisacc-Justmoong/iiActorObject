#include <iiActorObject/MeshTopologyCore.h>
#include <type_traits>

static_assert(std::is_default_constructible_v<iiActorObject::MeshTopology>);
static_assert(std::is_default_constructible_v<iiActorObject::TopologyRenderData>);

int meshTopologyCoreHeaderProbe()
{
    iiActorObject::MeshTopologyDefinition definition;
    return definition.sourceElement == iiActorObject::NoTopologyIndex ? 0 : 1;
}
