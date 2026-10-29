#include "../src/Project/Object3DProjectHistorySelection.h"
#include <iiActorObject/Object3DData.h>
#include <array>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
template<class F>void rejects(F operation)
{try{operation();}catch(const std::invalid_argument &){return;}throw std::runtime_error("Invalid history selection transition accepted");}
Object3DMesh mesh(std::string name)
{
    Object3DMesh result;result.name=std::move(name);result.geometry.vertices.resize(3);
    result.geometry.triangles={0,1,2};return result;
}
PointSelectionState selection(std::string name,std::size_t meshIndex)
{
    PointSelectionState state;
    state.entries.push_back({std::move(name),std::make_shared<const std::vector<MeshPointMask>>(
        std::vector<MeshPointMask>{{meshIndex,{1,.5,0}}})});
    return state;
}
}
int main()
{
    Object3DData before;before.meshes={mesh("A"),mesh("B")};
    Object3DData removed;removed.meshes={before.meshes[0]};
    Object3DData inserted;inserted.meshes={before.meshes[0],mesh("C"),before.meshes[1]};
    Object3DData mixed;mixed.meshes={mesh("C"),before.meshes[0]};
    auto old=selection("Face",0),same=old,renamed=selection("Body",0),moved=selection("Face",1);
    const ProjectHistorySelectionView prior{before,&old,true},removal{removed,&same,true};
    const ProjectHistorySelectionView insertion{inserted,&same,true},replacement{mixed,&moved,true};
    const std::array<std::optional<std::size_t>,2> removeMap{0,std::nullopt},insertMap{0,2},mixedMap{1,std::nullopt};
    validateProjectHistorySelectionTransition(33,&prior,removal,false,removeMap);
    validateProjectHistorySelectionTransition(34,&prior,insertion,false,insertMap);
    validateProjectHistorySelectionTransition(35,&prior,replacement,false,mixedMap);
    rejects([&]{validateProjectHistorySelectionTransition(32,&prior,removal,false,removeMap);});
    rejects([&]{validateProjectHistorySelectionTransition(33,&prior,insertion,false,insertMap);});
    rejects([&]{validateProjectHistorySelectionTransition(34,&prior,replacement,false,mixedMap);});
    rejects([&]{validateProjectHistorySelectionTransition(35,nullptr,replacement,false,mixedMap);});
    const ProjectHistorySelectionView noLayer{removed,&same,false},noSelection{removed,nullptr,true};
    rejects([&]{validateProjectHistorySelectionTransition(33,&prior,noLayer,false,removeMap);});
    rejects([&]{validateProjectHistorySelectionTransition(33,&prior,noSelection,false,removeMap);});
    const ProjectHistorySelectionView wrongName{removed,&renamed,true};
    rejects([&]{validateProjectHistorySelectionTransition(33,&prior,wrongName,false,removeMap);});
    const std::array<std::optional<std::size_t>,2> duplicateMap{0,0};
    rejects([&]{validateProjectHistorySelectionTransition(35,&prior,removal,false,duplicateMap);});

    const ProjectHistorySelectionView transfer{removed,&same,false};
    validateProjectHistorySelectionTransition(28,&prior,transfer,false,{});
    rejects([&]{validateProjectHistorySelectionTransition(27,&prior,transfer,false,{});});
    const ProjectHistorySelectionView renamedTransfer{removed,&renamed,false};
    rejects([&]{validateProjectHistorySelectionTransition(28,&prior,renamedTransfer,false,{});});
    rejects([&]{validateProjectHistorySelectionTransition(28,&prior,removal,false,{});});
    validateProjectHistorySelectionTransition(1,&prior,removal,true,{});

    const ProjectHistorySelectionView unchanged{removed,&old,true};
    validateProjectHistorySelectionTransition(1,&prior,unchanged,false,{});
    auto altered=removed;altered.meshes[0].geometry.triangles={0,2,1};
    const ProjectHistorySelectionView changed{altered,&old,true};
    rejects([&]{validateProjectHistorySelectionTransition(1,&prior,changed,false,{});});
    validateProjectHistorySelectionTransition(1,nullptr,unchanged,false,{});
}
