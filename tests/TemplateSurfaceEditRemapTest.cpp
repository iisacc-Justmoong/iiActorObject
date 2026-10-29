#include "../src/Geometry/Object3DTemplateSurfaceEditRemap.h"
#include "../src/Core/Object3DAdjustmentData.h"
#include <array>
#include <memory>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool value,const std::source_location &where=std::source_location::current())
{if(!value)throw std::runtime_error("Template surface edit remap failed at line "+std::to_string(where.line()));}
template<class Error=std::invalid_argument,class F>void rejects(F operation,std::string_view message={})
{
    try{operation();}
    catch(const Error &error)
    {if(!message.empty()&&error.what()!=message)throw std::runtime_error("Expected: "+std::string(message)+"; received: "+error.what());return;}
    throw std::runtime_error("Invalid surface edit remap accepted");
}
}
int main()
{
    using Rows=std::vector<MeshPointMask>;
    const std::vector<std::optional<std::size_t>> mapping{0,std::nullopt,1,2};
    const std::set<std::size_t> removed{1};
    const TemplateSurfaceEditRemapper mixed(mapping,removed,false,TemplateSurfaceRemapMode::Change);
    const TemplateSurfaceEditRemapper discard(mapping,removed,true,TemplateSurfaceRemapMode::Change);
    const TemplateSurfaceEditRemapper removal(mapping,removed,false,TemplateSurfaceRemapMode::Removal);
    const TemplateSurfaceEditRemapper removeDiscard(mapping,removed,true,TemplateSurfaceRemapMode::Removal);
    const auto noMasks=mixed.masks({},TemplateSurfaceMaskKind::Layer);
    check(!noMasks.value&&!noMasks.changed);
    const auto unchangedMasks=std::make_shared<const Rows>(Rows{{0,{.25,.75}}});
    const auto unchanged=mixed.masks(unchangedMasks,TemplateSurfaceMaskKind::Layer);
    check(!unchanged.changed&&unchanged.value==unchangedMasks);
    const auto empty=std::make_shared<const Rows>();
    check(mixed.masks(empty,TemplateSurfaceMaskKind::NamedSelection).value==empty);
    const auto masks=std::make_shared<const Rows>(Rows{{0,{.5}},{2,{.25,.75}},{3,{1}}});
    for(const auto *planner:{&mixed,&removal})
    {
        const auto result=planner->masks(masks,TemplateSurfaceMaskKind::Layer);
        check(result.changed&&result.value&&result.value!=masks);
        check(*result.value==Rows({{0,{.5}},{1,{.25,.75}},{2,{1}}}));
        check((*masks)[1].mesh==2&&(*masks)[1].weights==std::vector<double>({.25,.75}));
    }
    const auto removedMasks=std::make_shared<const Rows>(Rows{{1,{1}},{2,{.3}}});
    rejects([&]{(void)mixed.masks(removedMasks,TemplateSurfaceMaskKind::Layer);},"Removed surface has layer masks");
    rejects([&]{(void)removal.masks(removedMasks,TemplateSurfaceMaskKind::NamedSelection);},"Removed surface has a named point selection");
    for(const auto *planner:{&discard,&removeDiscard})
    {
        const auto result=planner->masks(removedMasks,TemplateSurfaceMaskKind::NamedSelection);
        check(result.changed&&*result.value==Rows({{1,{.3}}})&&removedMasks->size()==2);
        const auto only=std::make_shared<const Rows>(Rows{{1,{1}}});
        const auto cleared=planner->masks(only,TemplateSurfaceMaskKind::Layer);
        check(cleared.changed&&!cleared.value&&only->size()==1);
    }

    auto value=std::make_shared<Adjustment>();value->name="Original";
    value->meshes.push_back({});value->meshes.back().mesh=0;
    const std::shared_ptr<const Adjustment> immutable=value;
    const auto stable=mixed.adjustment(immutable);
    check(stable.value==immutable&&!stable.writable);
    auto resources=std::make_shared<Adjustment>(*value);resources->materials.emplace(1);resources->materials->front().name="Old";
    const std::shared_ptr<const Adjustment> resourceInput=resources;
    const auto resourceCopy=mixed.adjustment(resourceInput);
    check(resourceCopy.writable&&resourceCopy.value.get()==resourceCopy.writable.get()&&resourceCopy.value!=resourceInput);
    resourceCopy.writable->materials->front().name="New";
    check(resources->materials->front().name=="Old");
    const auto resourceNoCopy=removal.adjustment(resourceInput);
    check(resourceNoCopy.value==resourceInput&&!resourceNoCopy.writable);
    auto imageResources=std::make_shared<Adjustment>(*value);imageResources->images.emplace();
    const std::shared_ptr<const Adjustment> imageInput=imageResources;
    check(mixed.adjustment(imageInput).writable&&removal.adjustment(imageInput).value==imageInput);

    auto adjusted=std::make_shared<Adjustment>();adjusted->name="Edited";
    adjusted->meshes.push_back({});adjusted->meshes.back().mesh=0;
    adjusted->meshes.push_back({});adjusted->meshes.back().mesh=2;
    adjusted->meshes.back().positions.push_back({4,{0,.5,0}});
    const std::shared_ptr<const Adjustment> edited=adjusted;
    for(const auto *planner:{&mixed,&removal})
    {
        const auto result=planner->adjustment(edited);
        check(result.writable&&result.value.get()==result.writable.get()&&result.value!=edited);
        check(result.value->meshes[1].mesh==1&&result.value->meshes[1].positions[0].position.y==.5);
        check(adjusted->meshes[1].mesh==2&&result.value->name=="Edited");
    }
    auto doomed=std::make_shared<Adjustment>();doomed->meshes.push_back({});doomed->meshes.back().mesh=1;
    const std::shared_ptr<const Adjustment> doomedInput=doomed;
    rejects([&]{(void)mixed.adjustment(doomedInput);},"Removed surface has edit layers");
    rejects([&]{(void)removal.adjustment(doomedInput);},"Removed surface has edit layers");
    for(const auto *planner:{&discard,&removeDiscard})
    {
        const auto result=planner->adjustment(doomedInput);
        check(result.writable&&result.value->meshes.empty()&&doomed->meshes.size()==1);
    }
    rejects([&]{(void)mixed.adjustment({});},"Missing template surface adjustment");

    const auto malformed=std::make_shared<const Rows>(Rows{{8,{1}},{1,{1}}});
    rejects([&]{(void)mixed.masks(malformed,TemplateSurfaceMaskKind::Layer);},"Removed surface has layer masks");
    rejects<std::out_of_range>([&]{(void)removal.masks(malformed,TemplateSurfaceMaskKind::Layer);});
    auto malformedAdjustment=std::make_shared<Adjustment>();
    malformedAdjustment->meshes.push_back({});malformedAdjustment->meshes.back().mesh=8;
    malformedAdjustment->meshes.push_back({});malformedAdjustment->meshes.back().mesh=1;
    rejects([&]{(void)mixed.adjustment(malformedAdjustment);},"Removed surface has edit layers");
    rejects<std::out_of_range>([&]{(void)removal.adjustment(malformedAdjustment);});
    check(malformedAdjustment->meshes[0].mesh==8&&malformedAdjustment->meshes[1].mesh==1);
    const std::set<std::size_t> none;
    const TemplateSurfaceEditRemapper missing(mapping,none,true,TemplateSurfaceRemapMode::Change);
    rejects([&]{(void)missing.masks(std::make_shared<const Rows>(Rows{{1,{1}}}),TemplateSurfaceMaskKind::Layer);},
        "Removed surface is required by another template feature");
    const TemplateSurfaceEditRemapper missingRemoval(mapping,none,true,TemplateSurfaceRemapMode::Removal);
    rejects([&]{(void)missingRemoval.adjustment(doomedInput);},"Removed surface is required by another template feature");
    rejects([&]{(void)mixed.masks({},static_cast<TemplateSurfaceMaskKind>(-1));},"Unknown template surface mask kind");
    rejects([&]{(void)TemplateSurfaceEditRemapper(mapping,removed,false,static_cast<TemplateSurfaceRemapMode>(-1));},
        "Unknown template surface edit remap mode");
}
