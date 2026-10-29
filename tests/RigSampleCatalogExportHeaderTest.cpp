#include <iiActorObject/RigSampleCatalogExport.h>
#include <stdexcept>

int rigSampleCatalogExportHeaderProbe()
{
    if(iiActorObject::exportRigSampleCatalogsJson({})!="{\"schemaVersion\":1,\"catalogs\":[]}")return 1;
    try {(void)iiActorObject::exportRigSampleCatalogsJson({},2);}
    catch(const std::length_error &){return 0;}
    return 1;
}
