#include <iiActorObject/RigSampleCatalog.h>
#include <type_traits>

static_assert(std::is_default_constructible_v<iiActorObject::RigSampleCatalog>);
int rigSampleCatalogCoreHeaderProbe();
int rigSampleCatalogExportHeaderProbe();
int main()
{
    return rigSampleCatalogCoreHeaderProbe() || rigSampleCatalogExportHeaderProbe();
}
