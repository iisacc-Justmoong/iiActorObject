#include "MakeHumanTemplate.h"
#include "HumanSkinInternal.h"
#include "MakeHumanResourceReader.h"
#include "HumanAccessoryCatalogInternal.h"
#include "MakeHumanMorphology.h"
#include "MakeHumanExpressions.h"
#include "MakeHumanAttachments.h"
#include "MakeHumanRigReader.h"
#include <iiActorObject/Object3DObj.h>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace iiActorObject::NSCharacter
{
namespace
{
using human_detail::Value;
using human_detail::Files;
using human_detail::field;
using human_detail::as;
using human_detail::AccessoryEntry;
using human_detail::accessoryCatalog;
using human_detail::selectAccessories;
using human_detail::semanticControls;
using human_detail::macroAssets;
using human_detail::expressionAssets;
using human_detail::faceAssets;
using human_detail::accessoryAssets;
using human_detail::loadMakeHumanRig;

}
static CharacterTemplate loadMakeHumanTemplateImpl(const Object3DReference &engine,const std::filesystem::path &directory,
                                         const MakeHumanTemplateOptions &options,const HumanSkinSelection *skin)
{
    if(!engine.valid())throw std::invalid_argument("Template import requires a live core engine");
    auto initialBudget=options.maximumBytes;std::optional<human_skin_detail::Prepared> prepared;
    if(skin){prepared=human_skin_detail::prepare(*skin,initialBudget);initialBudget-=prepared->bytesRead;}
    std::optional<std::vector<AccessoryEntry>> accessories;
    if(options.accessories)
    {
        Files catalogueFiles(options.accessories->directory,initialBudget);
        accessories=selectAccessories(accessoryCatalog(catalogueFiles),*options.accessories);initialBudget=catalogueFiles.remaining();
    }
    Files files(directory,initialBudget);Object3DObjOptions obj;obj.scale=options.scale;obj.visibleGroups=options.visibleGroups;
    auto data=parseObject3DObj(files.read("base.obj"),obj);data.name="CC0Human";
    auto &topology=*data.meshes[0].topology;
    if(options.groundAtFeet)
    {
        double floor=std::numeric_limits<double>::max();const auto &layout=topology.layout->definition();
        for(const auto r:layout.renderToLayout)floor=std::min(floor,topology.points[layout.layouts[r].position].position.y);
        for(auto &p:topology.points)p.position.y-=floor;
    }
    Object3DTemplateDefinition definition;loadMakeHumanRig(data,definition,files);
    const auto catalog=files.json("catalog.json");
    if(as<std::string>(field(catalog,"format"))!="iiActorObject.MakeHumanTemplate.1")throw std::invalid_argument("Unsupported human catalog");
    const auto &targets=as<Value::Array>(field(catalog,"targets"));
    if(targets.size()>4096)throw std::length_error("Human target catalog limit");
    for(const auto &target:targets)
    {
        TemplateTarget t;t.name=as<std::string>(field(target,"name"));
        t.deltas=parseObject3DTarget(files.read(as<std::string>(field(target,"path"))),topology.points.size(),options.scale);
        definition.targets.push_back(std::move(t));
    }
    if(options.semanticControls)semanticControls(definition,files.json("modeling_modifiers.json"));
    auto remaining=files.remaining();
    if(!options.macroAssets.empty())
    {
        Files macros(options.macroAssets,remaining);macroAssets(data,definition,macros,options.scale);remaining=macros.remaining();
        if(options.groundAtFeet){definition.grounding=TemplateGrounding{};(void)groundObject3D(data,*definition.grounding);}
    }
    bindObject3DTemplateJoints(data,definition.joints);
    if(!options.faceAssets.empty()){Files faces(options.faceAssets,remaining);faceAssets(data,definition,faces,options.scale);remaining=faces.remaining();}
    if(accessories){Files filesForAccessories(options.accessories->directory,remaining);accessoryAssets(data,definition,filesForAccessories,options.scale,*accessories,options.accessories->hideCoveredBody);remaining=filesForAccessories.remaining();}
    if(!options.expressionAssets.empty()){Files expressions(options.expressionAssets,remaining);definition.poseProgram=expressionAssets(data,expressions,options.scale);remaining=expressions.remaining();}
    if(prepared)(void)human_skin_detail::applyPrepared(data,skin->mesh,std::move(*prepared));
    definition.base=engine.createObject(std::move(data));const auto baseId=definition.base.id();
    try { return CharacterTemplate(engine.createTemplate(std::move(definition))); }
    catch(...) { engine.eraseObject(baseId);throw; }
}
CharacterTemplate loadMakeHumanTemplate(const Object3DReference &engine,const std::filesystem::path &directory,
                                         const MakeHumanTemplateOptions &options)
{return loadMakeHumanTemplateImpl(engine,directory,options,nullptr);}
CharacterTemplate loadMakeHumanTemplate(const Object3DReference &engine,const std::filesystem::path &directory,
                                         const MakeHumanTemplateOptions &options,const HumanSkinSelection &skin)
{return loadMakeHumanTemplateImpl(engine,directory,options,&skin);}
} // namespace iiActorObject::NSCharacter
