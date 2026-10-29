#include "../src/Geometry/Object3DTemplateSurfaceMaterialRouting.h"
#include <array>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <source_location>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool value,const std::source_location &where=std::source_location::current())
{if(!value)throw std::runtime_error("Surface material routing test failed at line "+std::to_string(where.line()));}
template<class F>void rejects(F action)
{try{action();}catch(const std::exception &){return;}throw std::runtime_error("Invalid surface material routing accepted");}
MeshTopology topology(std::vector<std::uint32_t> materials)
{
    MeshTopology result;
    result.points={{{0,0,0},{}},{{1,0,0},{}},{{0,1,0},{}},
                   {{2,0,0},{}},{{3,0,0},{}},{{2,1,0},{}}};
    MeshTopologyDefinition definition;definition.positionCount=6;
    definition.layouts={{0},{1},{2},{3},{4},{5}};
    definition.polygons={{0,1,2},{3,4,5}};definition.polygonMaterials=std::move(materials);
    result.layout=MeshTopologyLayout::create(std::move(definition),result.points);
    return result;
}
struct StubMapper final:TemplateSurfaceMaterialMapper
{
    std::vector<std::tuple<char,std::size_t,std::size_t>> calls;
    std::map<std::size_t,std::size_t> mapping;
    void linkMaterial(std::size_t candidate,std::size_t existing) override
    {calls.emplace_back('L',candidate,existing);}
    void linkRetainedImages() override {calls.emplace_back('I',0,0);}
    std::size_t material(std::size_t candidate) override
    {calls.emplace_back('M',candidate,0);return mapping.at(candidate);}
};
}
int main()
{
    Object3DMesh oldAuth,newAuth,oldRender,newRender,extra;
    oldAuth.material=1;oldAuth.topology=topology({1,2});
    newAuth.material=7;newAuth.topology=topology({7,8});
    oldRender.material=3;oldRender.geometry.triangles={0,1,2,0,2,3};oldRender.triangleMaterials={3,4};
    newRender.material=9;newRender.geometry.triangles=oldRender.geometry.triangles;
    const std::vector<Object3DMesh> before{oldAuth,oldRender},after{newAuth,extra,newRender};
    const std::array<std::optional<std::size_t>,3> mapping{0,std::nullopt,1};
    StubMapper mapper;
    linkRetainedTemplateSurfaceMaterials(before,after,mapping,mapper);
    check(mapper.calls==std::vector<std::tuple<char,std::size_t,std::size_t>>({
        {'L',7,1},{'L',7,1},{'L',8,2},{'L',9,3},{'L',9,3},{'L',9,4},{'I',0,0}}));
    auto fallbackBefore=oldAuth,fallbackAfter=newAuth;
    fallbackBefore.topology=topology({});fallbackAfter.topology=topology({7,8});
    mapper.calls.clear();
    linkRetainedTemplateSurfaceMaterials(std::span(&fallbackBefore,1),std::span(&fallbackAfter,1),
        std::array<std::optional<std::size_t>,1>{0},mapper);
    check(mapper.calls==std::vector<std::tuple<char,std::size_t,std::size_t>>({
        {'L',7,1},{'L',7,1},{'L',8,1},{'I',0,0}}));
    fallbackAfter.topology=topology({});mapper.calls.clear();
    linkRetainedTemplateSurfaceMaterials(std::span(&fallbackBefore,1),std::span(&fallbackAfter,1),
        std::array<std::optional<std::size_t>,1>{0},mapper);
    check(mapper.calls==std::vector<std::tuple<char,std::size_t,std::size_t>>({{'L',7,1},{'I',0,0}}));
    mapper.calls.clear();
    rejects([&]{linkRetainedTemplateSurfaceMaterials(before,after,std::array<std::optional<std::size_t>,1>{0},mapper);});
    check(mapper.calls.empty());
    rejects([&]{linkRetainedTemplateSurfaceMaterials(before,after,
        std::array<std::optional<std::size_t>,3>{0,std::nullopt,9},mapper);});
    auto missing=newAuth;missing.topology->layout.reset();
    rejects([&]{linkRetainedTemplateSurfaceMaterials(std::span(&oldAuth,1),std::span(&missing,1),
        std::array<std::optional<std::size_t>,1>{0},mapper);});

    Object3DMesh added=newAuth;added.triangleMaterials={7,8};std::vector<Object3DMesh> staged{oldAuth,added};
    mapper.calls.clear();mapper.mapping={{7,11},{8,12}};
    remapAddedTemplateSurfaceMaterials(staged,std::set<std::size_t>{1},mapper);
    check(staged[1].material==11&&staged[1].triangleMaterials==std::vector<std::size_t>({11,12}));
    check(staged[1].topology->layout->definition().polygonMaterials==std::vector<std::uint32_t>({11,12}));
    check(staged[0].material==1&&added.material==7&&added.topology->layout->definition().polygonMaterials[0]==7);
    check(mapper.calls==std::vector<std::tuple<char,std::size_t,std::size_t>>({
        {'M',7,0},{'M',7,0},{'M',8,0},{'M',7,0},{'M',8,0}}));
    Object3DMesh render;render.material=7;render.triangleMaterials={7,8};staged={render};
    remapAddedTemplateSurfaceMaterials(staged,std::set<std::size_t>{0},mapper);
    check(staged[0].material==11&&staged[0].triangleMaterials==std::vector<std::size_t>({11,12}));
    staged={added};mapper.mapping={{7,11}};
    rejects([&]{remapAddedTemplateSurfaceMaterials(staged,std::set<std::size_t>{0},mapper);});
    check(staged[0].material==11&&staged[0].triangleMaterials[0]==11&&added.material==7);

    auto authored=added;authored.topology->sourceTangents=std::make_shared<TopologyTangentSource>();
    const auto layout=authored.topology->layout;staged={authored};mapper.mapping={{7,7},{8,8}};mapper.calls.clear();
    remapAddedTemplateSurfaceMaterials(staged,std::set<std::size_t>{0},mapper);
    check(staged[0].topology->layout==layout&&staged[0].topology->sourceTangents==authored.topology->sourceTangents);
    staged={authored};mapper.mapping={{7,11},{8,12}};
    rejects([&]{remapAddedTemplateSurfaceMaterials(staged,std::set<std::size_t>{0},mapper);});
    check(staged[0].material==11&&authored.material==7);
    authored.topology->sourceTangents.reset();authored.topology->sourceGlbTangents=std::make_shared<TopologyGlbTangentSource>();
    staged={authored};rejects([&]{remapAddedTemplateSurfaceMaterials(staged,std::set<std::size_t>{0},mapper);});
    staged={added};rejects([&]{remapAddedTemplateSurfaceMaterials(staged,std::set<std::size_t>{2},mapper);});
    auto missingAdded=added;missingAdded.topology->layout.reset();staged={missingAdded};
    rejects([&]{remapAddedTemplateSurfaceMaterials(staged,std::set<std::size_t>{0},mapper);});
    if constexpr(std::numeric_limits<std::size_t>::max()>std::numeric_limits<std::uint32_t>::max())
    {
        staged={added};mapper.mapping={{7,std::numeric_limits<std::size_t>::max()},{8,12}};
        rejects([&]{remapAddedTemplateSurfaceMaterials(staged,std::set<std::size_t>{0},mapper);});
    }
}
