#include "../src/Interchange/FBX/Object3DFbxTopologyRead.h"
#include "../src/Interchange/FBX/Object3DFbxTopologyWrite.h"
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
using namespace iiActorObject::object3d_detail::fbx_binary;

namespace
{
void check(bool condition)
{
    if (!condition) throw std::runtime_error("FBX topology metadata contract failed");
}
template<class F> void rejects(F operation)
{
    try { operation(); }
    catch (const std::exception &) { return; }
    throw std::runtime_error("Invalid topology metadata was accepted");
}
const Node &child(const Node &parent,std::string_view name)
{
    for (const auto &node:parent.children) if (node.name == name) return node;
    throw std::runtime_error("Missing FBX topology field");
}
struct Fixture final
{
    static constexpr std::array names{"Version","Format","Element","PositionCount","UvCount","NormalCount",
        "Reverse","Layouts","PolygonSizes","Polygons","Hidden","RenderToLayout","Materials","Groups"};
    std::array<ufbx_dom_value,names.size()> values{};
    std::array<ufbx_dom_node,names.size()> fields{};
    std::array<ufbx_dom_node *,names.size()> pointers{};
    ufbx_dom_node root{};
    std::array<std::uint8_t,36> layouts{0,0,0,0,255,255,255,255,255,255,255,255,
        1,0,0,0,255,255,255,255,255,255,255,255,2,0,0,0,255,255,255,255,255,255,255,255};
    std::array<std::uint8_t,4> sizes{3,0,0,0},materials{0,0,0,0};
    std::array<std::uint8_t,12> indices{0,0,0,0,1,0,0,0,2,0,0,0};
    std::array<std::uint8_t,8> groups{4,0,0,0,'B','o','d','y'};
    Fixture()
    {
        for (std::size_t i=0;i<names.size();++i)
        {
            fields[i].name={names[i],std::strlen(names[i])};
            fields[i].values={&values[i],1};pointers[i]=&fields[i];
        }
        root.children={pointers.data(),pointers.size()};
        number(0,1);number(2,7);number(3,3);number(4,0);number(5,0);number(6,0);
        values[1].type=UFBX_DOM_VALUE_STRING;values[1].value_str={"FBX",3};
        blob(7,layouts);blob(8,sizes);blob(9,indices);blob(10,std::span<const std::uint8_t>{});
        blob(11,indices);blob(12,materials);blob(13,groups);
    }
    void number(std::size_t i,std::int64_t n)
    {values[i].type=UFBX_DOM_VALUE_NUMBER;values[i].value_int=n;values[i].value_float=static_cast<double>(n);}
    void blob(std::size_t i,std::span<const std::uint8_t> bytes)
    {values[i].type=UFBX_DOM_VALUE_STRING;values[i].value_blob={bytes.data(),bytes.size()};}
};
}
int main()
{
    Fixture fixture;
    const FbxTopologyReadLimits limits{3,1,3};
    const auto definition=readFbxTopologyMetadata(&fixture.root,limits);
    check(definition.sourceFormat=="FBX" && definition.sourceElement==7 && definition.positionCount==3);
    check(definition.layouts.size()==3 && definition.layouts[2].position==2);
    check(definition.polygons==std::vector<std::vector<std::uint32_t>>({{0,1,2}}));
    check(definition.polygonGroups==std::vector<std::string>({"Body"}) && definition.polygonMaterials==std::vector<std::uint32_t>({0}));
    check(definition.renderToLayout==std::vector<std::uint32_t>({0,1,2}));
    fixture.number(0,2);
    rejects([&] { (void)readFbxTopologyMetadata(&fixture.root,limits); });
    fixture.number(0,1);
    fixture.fields[0].values.data=nullptr;
    rejects([&] { (void)readFbxTopologyMetadata(&fixture.root,limits); });
    fixture.fields[0].values.data=&fixture.values[0];
    rejects([&] { (void)readFbxTopologyMetadata(&fixture.root,FbxTopologyReadLimits{2,1,3}); });
    rejects([&] { (void)readFbxTopologyMetadata(&fixture.root,FbxTopologyReadLimits{std::numeric_limits<std::size_t>::max(),1,3}); });
    fixture.groups[0]=9;
    rejects([&] { (void)readFbxTopologyMetadata(&fixture.root,limits); });
    fixture.groups[0]=4;
    fixture.values[13].value_blob.data=nullptr;
    rejects([&] { (void)readFbxTopologyMetadata(&fixture.root,limits); });
    fixture.blob(13,fixture.groups);
    fixture.values[7].value_blob.data=nullptr;
    rejects([&] { (void)readFbxTopologyMetadata(&fixture.root,limits); });
    fixture.blob(7,fixture.layouts);
    fixture.values[1].value_str.data=nullptr;
    rejects([&] { (void)readFbxTopologyMetadata(&fixture.root,limits); });
    fixture.values[1].value_str={"FBX",3};
    rejects([] { (void)writeFbxTopologyMetadata(MeshTopology{}); });

    MeshTopology topology;
    topology.points={{{0,0,0},{}},{{1,0,0},{}},{{0,1,0},{}}};
    topology.layout=MeshTopologyLayout::create(definition,topology.points);
    topology.additionalUvSets.push_back({"Extra",{}, {0,1,2}});
    const auto written=writeFbxTopologyMetadata(topology);
    check(written.name=="iiActorObjectTopology");
    check(child(written,"Version").properties[0].bytes==Bytes({'I',1,0,0,0}));
    check(child(written,"Groups").properties[0].bytes==Bytes({'R',8,0,0,0,4,0,0,0,'B','o','d','y'}));
    check(child(written,"EmptyWeights").properties[0].bytes==Bytes({'R',12,0,0,0,0,0,0,0,1,0,0,0,2,0,0,0}));
    const auto &uv=child(written,"AdditionalUvSet");
    check(child(uv,"Name").properties[0].bytes==Bytes({'S',5,0,0,0,'E','x','t','r','a'}));
    check(child(uv,"Layouts").properties[0].bytes==Bytes({'R',12,0,0,0,0,0,0,0,1,0,0,0,2,0,0,0}));
}
