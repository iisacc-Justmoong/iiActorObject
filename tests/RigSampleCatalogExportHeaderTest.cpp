#include <iiActorObject/RigSampleCatalogExport.h>
#include <array>
#include <locale>
#include <numbers>
#include <sstream>
#include <stdexcept>

int rigSampleCatalogExportHeaderProbe()
{
    if(iiActorObject::exportRigSampleCatalogsJson({})!="{\"schemaVersion\":1,\"catalogs\":[]}")return 1;
    try {(void)iiActorObject::exportRigSampleCatalogsJson({},2);return 1;}
    catch(const std::length_error &){}
    iiActorObject::RigSampleCatalog catalog;
    catalog.sourceFormat="test";
    catalog.sourceBones={{"root",iiActorObject::NoBone,{}}};
    catalog.rawControls={"weight"};
    catalog.rawControlDefaults={std::numbers::pi};
    catalog.baseline.controls={std::numbers::pi};
    catalog.baseline.sourceLocals.resize(1);
    catalog.samples={{"morph",catalog.baseline}};
    struct Comma final:std::numpunct<char>{char do_decimal_point() const override{return ',';}};
    const auto old=std::locale();
    const auto plain=iiActorObject::exportRigSampleCatalogsJson(std::span(&catalog,1));
    std::locale::global(std::locale(old,new Comma));
    const auto localized=iiActorObject::exportRigSampleCatalogsJson(std::span(&catalog,1));
    std::locale::global(old);
    if(localized!=plain)return 1;
    const auto key=std::string_view("\"rawControlDefaults\":[");
    const auto start=plain.find(key);
    if(start==std::string::npos)return 1;
    std::istringstream input(plain.substr(start+key.size()));input.imbue(std::locale::classic());
    double value=0;input>>value;
    return !input||value!=std::numbers::pi;
}
