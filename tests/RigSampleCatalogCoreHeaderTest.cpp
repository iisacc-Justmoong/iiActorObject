#include <iiActorObject/RigSampleCatalogCore.h>
#include <stdexcept>

int rigSampleCatalogCoreHeaderProbe()
{
    iiActorObject::RigSampleCatalog catalog;
    try {iiActorObject::validateRigSampleCatalogs(std::span(&catalog,1));}
    catch(const std::invalid_argument &){return 0;}
    return 1;
}
