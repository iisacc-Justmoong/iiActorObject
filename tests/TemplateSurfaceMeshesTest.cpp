#include "../src/Geometry/Object3DTemplateSurfaceMeshes.h"
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
{if(!value)throw std::runtime_error("Template surface meshes test failed at line "+std::to_string(where.line()));}
template<class F>void rejects(F action)
{try{action();}catch(const std::exception &){return;}throw std::runtime_error("Invalid template surface assembly accepted");}
MeshTopology topology(bool hidden,std::uint32_t materialA,std::uint32_t materialB)
{
    MeshTopology result;
    result.points={{{0,0,0},{}},{{1,0,0},{}},{{0,1,0},{}},
                   {{2,0,0},{}},{{3,0,0},{}},{{2,1,0},{}}};
    MeshTopologyDefinition definition;definition.positionCount=6;
    definition.layouts={{0},{1},{2},{3},{4},{5}};
    definition.polygons={{0,1,2},{3,4,5}};definition.polygonMaterials={materialA,materialB};
    if(hidden)definition.hiddenPolygons={0};
    result.layout=MeshTopologyLayout::create(std::move(definition),result.points);
    return result;
}
Object3DMesh mesh(std::string name,std::size_t material,bool hidden=false)
{
    Object3DMesh result;result.name=std::move(name);result.material=material;
    result.topology=topology(hidden,static_cast<std::uint32_t>(material),static_cast<std::uint32_t>(material));
    return result;
}
}
int main()
{
    const auto body=mesh("Body",1),old=mesh("Old",2),kept=mesh("Kept",3);
    const std::vector<Object3DMesh> before{body,old,kept};
    auto candidateBody=mesh("Body",8,true);
    candidateBody.topology->points[0].position.x=42;
    auto added=mesh("Added",7);added.triangleMaterials={7,7};
    const std::vector<Object3DMesh> after{candidateBody,added,kept};
    const std::array<std::optional<std::size_t>,3> mapping{0,std::nullopt,2};
    const std::set<std::size_t> addedSlots{1},visibility{0};
    const auto mixed=assembleTemplateSurfaceMeshes(before,after,mapping,addedSlots,visibility);
    check(mixed.size()==3&&mixed[0].name=="Body"&&mixed[1].name=="Added"&&mixed[2].name=="Kept");
    check(mixed[0].topology->layout->definition().hiddenPolygons==std::vector<std::uint32_t>{0});
    check(mixed[0].topology->layout->definition().polygonMaterials==std::vector<std::uint32_t>({1,1}));
    check(mixed[0].topology->points[0].position.x==0&&after[0].topology->points[0].position.x==42);
    check(mixed[1].material==7&&mixed[1].triangleMaterials==std::vector<std::size_t>({7,7}));
    check(mixed[2].topology->layout==before[2].topology->layout);
    check(before[0].topology->layout->definition().hiddenPolygons.empty());

    const std::array<std::optional<std::size_t>,2> removalMapping{0,2};
    const std::vector<Object3DMesh> removalCandidate{candidateBody,kept};
    const auto removed=assembleTemplateSurfaceMeshes(before,removalCandidate,removalMapping,{},visibility);
    check(removed.size()==2&&removed[1].name=="Kept"&&removed[0].topology->layout->definition().hiddenPolygons[0]==0);
    auto unchanged=templateSurfaceRetainedMesh(before[0],after[0],false);
    check(unchanged.topology->layout==before[0].topology->layout);

    rejects([&]{(void)assembleTemplateSurfaceMeshes(before,after,std::array<std::optional<std::size_t>,1>{0},addedSlots,visibility);});
    rejects([&]{(void)assembleTemplateSurfaceMeshes(before,after,std::array<std::optional<std::size_t>,3>{0,1,2},addedSlots,visibility);});
    rejects([&]{(void)assembleTemplateSurfaceMeshes(before,after,std::array<std::optional<std::size_t>,3>{0,std::nullopt,std::nullopt},addedSlots,visibility);});
    rejects([&]{(void)assembleTemplateSurfaceMeshes(before,after,std::array<std::optional<std::size_t>,3>{0,std::nullopt,9},addedSlots,visibility);});
    rejects([&]{(void)assembleTemplateSurfaceMeshes(before,after,std::array<std::optional<std::size_t>,3>{2,std::nullopt,0},addedSlots,{});});
    rejects([&]{(void)assembleTemplateSurfaceMeshes(before,after,mapping,addedSlots,std::set<std::size_t>{1});});
    rejects([&]{(void)assembleTemplateSurfaceMeshes(before,after,mapping,std::set<std::size_t>{1,4},visibility);});
    auto noTopology=after[0];noTopology.topology.reset();
    rejects([&]{(void)templateSurfaceRetainedMesh(before[0],noTopology,true);});
    check(before[0].topology->layout->definition().hiddenPolygons.empty());

    const auto replacement=templateSurfaceReplacementMesh(added,2);
    check(replacement.material==2&&replacement.triangleMaterials==std::vector<std::size_t>({2,2}));
    check(replacement.topology->layout->definition().polygonMaterials==std::vector<std::uint32_t>({2,2}));
    check(added.material==7&&added.topology->layout->definition().polygonMaterials==std::vector<std::uint32_t>({7,7}));
    auto authored=added;authored.topology->sourceTangents=std::make_shared<TopologyTangentSource>();
    rejects([&]{(void)templateSurfaceReplacementMesh(authored,2);});
    check(templateSurfaceReplacementMesh(authored,7).topology->sourceTangents==authored.topology->sourceTangents);
    authored.topology->sourceTangents.reset();authored.topology->sourceGlbTangents=std::make_shared<TopologyGlbTangentSource>();
    rejects([&]{(void)templateSurfaceReplacementMesh(authored,2);});
    auto missingLayout=added;missingLayout.topology->layout.reset();
    rejects([&]{(void)templateSurfaceReplacementMesh(missingLayout,2);});
    if constexpr(std::numeric_limits<std::size_t>::max()>std::numeric_limits<std::uint32_t>::max())
        rejects([&]{(void)templateSurfaceReplacementMesh(added,std::numeric_limits<std::size_t>::max());});
    Object3DMesh render;render.material=7;render.triangleMaterials={7,7};
    const auto renderReplacement=templateSurfaceReplacementMesh(render,2);
    check(renderReplacement.material==2&&renderReplacement.triangleMaterials==std::vector<std::size_t>({2,2}));
}
