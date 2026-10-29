#include "../src/Geometry/Object3DSelectionState.h"
#include "../src/Geometry/Object3DTemplateSurfaceSelectionRemap.h"
#include "../src/Geometry/Object3DTemplateSurfaceEditRemap.h"
#include <iiActorObject/MeshPointMaskValidation.h>
#include <iiActorObject/Object3DData.h>
#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool value,const std::source_location &where=std::source_location::current())
{if(!value)throw std::runtime_error("Point selection module test failed at line "+std::to_string(where.line()));}
template<class F>void rejects(F action)
{try{action();}catch(const std::exception &){return;}throw std::runtime_error("Invalid point selection accepted");}
Object3DMesh mesh(std::string name)
{
    Object3DMesh result;result.name=std::move(name);result.geometry.vertices.resize(3);
    result.geometry.triangles={0,1,2};return result;
}
using Rows=std::vector<MeshPointMask>;
}
int main()
{
    Object3DData before;before.meshes={mesh("A"),mesh("B"),mesh("C")};
    const auto stableMasks=std::make_shared<const Rows>(Rows{{0,{1,0,0}}});
    const auto selectionMasks=std::make_shared<const Rows>(Rows{{0,{0,.5,0}},{2,{0,1,0}}});
    auto state=std::make_shared<PointSelectionState>();
    state->entries={{"Blank",{}},{"Selected",selectionMasks},{"Stable",stableMasks}};
    validatePointSelectionState(before,*state);
    validateMeshPointMasks(before,*selectionMasks);
    rejects([&]{validateMeshPointMasks(before,Rows{{0,{1,0}},{0,{1,0,0}}});});
    rejects([&]{validateMeshPointMasks(before,Rows{{0,{1,0,0}},{0,{1,0,0}}});});
    rejects([&]{validateMeshPointMasks(before,Rows{{3,{1,0,0}}});});
    rejects([&]{validateMeshPointMasks(before,Rows{{0,{1,std::numeric_limits<double>::quiet_NaN(),0}}});});
    rejects([&]{validateMeshPointMasks(before,Rows{{0,{1,1.01,0}}});});
    Object3DData authored;authored.meshes.emplace_back();authored.meshes[0].geometry.vertices.resize(1);
    authored.meshes[0].topology.emplace();authored.meshes[0].topology->points.resize(4);
    validateMeshPointMasks(authored,Rows{{0,{1,0,0,.5}}});
    rejects([&]{validateMeshPointMasks(authored,Rows{{0,{1}}});});
    auto invalid=*state;invalid.entries[1].masks=std::make_shared<const Rows>(Rows{{0,{0,0,0}}});
    rejects([&]{validatePointSelectionState(before,invalid);});
    invalid=*state;invalid.entries[1].name="Blank";
    rejects([&]{validatePointSelectionState(before,invalid);});
    invalid.entries.clear();rejects([&]{validatePointSelectionState(before,invalid);});

    const std::vector<std::optional<std::size_t>> map{0,std::nullopt,1};
    const std::set<std::size_t> removed{1};
    const TemplateSurfaceEditRemapper remapper(map,removed,false,TemplateSurfaceRemapMode::Removal);
    const auto result=remapTemplateSurfaceSelections(state,remapper);
    check(result.changed&&result.value&&result.value.get()!=state.get());
    check(result.value->entries[0].name=="Blank"&&!result.value->entries[0].masks);
    check(result.value->entries[1].masks->at(1).mesh==1&&state->entries[1].masks->at(1).mesh==2);
    check(result.value->entries[2].masks==stableMasks);
    const auto empty=remapTemplateSurfaceSelections({},remapper);check(!empty.changed&&!empty.value);
    auto stable=std::make_shared<PointSelectionState>();stable->entries={{"Stable",stableMasks}};
    const auto same=remapTemplateSurfaceSelections(stable,remapper);check(!same.changed&&same.value.get()==stable.get());

    Object3DData afterRemoval;afterRemoval.meshes={before.meshes[0],before.meshes[2]};
    validateRemappedPointSelectionTransition(before,afterRemoval,*state,*result.value,map,SelectionRemapMode::Removal);
    auto afterMixed=afterRemoval;afterMixed.meshes.push_back(mesh("D"));
    validateRemappedPointSelectionTransition(before,afterMixed,*state,*result.value,map,SelectionRemapMode::Mixed);
    auto bad=*result.value;bad.entries[1].masks=std::make_shared<const Rows>(Rows{{0,{0,.5,0}},{1,{1,0,0}}});
    rejects([&]{validateRemappedPointSelectionTransition(before,afterRemoval,*state,bad,map,SelectionRemapMode::Removal);});
    auto changed=before;changed.meshes[0].geometry.triangles={0,2,1};
    rejects([&]{validatePointSelectionTransition(before,changed,*state);});

    const std::vector<std::optional<std::size_t>> insertionMap{0,2,3};
    const std::set<std::size_t> none;
    const TemplateSurfaceEditRemapper insertion(insertionMap,none,false,TemplateSurfaceRemapMode::Change);
    const auto inserted=remapTemplateSurfaceSelections(state,insertion);
    Object3DData afterInsertion;afterInsertion.meshes={before.meshes[0],mesh("D"),before.meshes[1],before.meshes[2]};
    validateRemappedPointSelectionTransition(before,afterInsertion,*state,*inserted.value,insertionMap,SelectionRemapMode::Insertion);
    check(inserted.value->entries[1].masks->at(1).mesh==3&&state->entries[1].masks->at(1).mesh==2);

    auto doomed=std::make_shared<PointSelectionState>(*state);
    doomed->entries[1].masks=std::make_shared<const Rows>(Rows{{1,{1,0,0}}});
    rejects([&]{(void)remapTemplateSurfaceSelections(doomed,remapper);});
    const TemplateSurfaceEditRemapper discard(map,removed,true,TemplateSurfaceRemapMode::Removal);
    const auto cleared=remapTemplateSurfaceSelections(doomed,discard);
    check(cleared.changed&&!cleared.value->entries[1].masks&&doomed->entries[1].masks->at(0).mesh==1);
    validateRemappedPointSelectionTransition(before,afterRemoval,*doomed,*cleared.value,map,SelectionRemapMode::Removal);
}
