#include "../src/Interchange/FBX/Object3DFbxExternalIdentityRead.h"
#include <iiActorObject/Object3DExternalIdentity.h>
#include <iiActorObject/MeshTopologyCore.h>
#include "../src/third_party/ufbx/ufbx.h"
#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool value)
{if(!value)throw std::runtime_error("FBX external identity reader contract failed");}
template<class F>void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid FBX identity accepted");}
void name(ufbx_prop &property,std::string_view key)
{property.name={key.data(),key.size()};}
void text(ufbx_prop &property,const std::string &value)
{property.type=UFBX_PROP_STRING;property.value_str={value.data(),value.size()};}

struct Fixture final
{
    MeshTopology topology;
    ufbx_node node{};
    ufbx_mesh source{};
    std::array<ufbx_prop,5> props{};
    std::array<std::string,4> values{
        "0,1,2,3","0","0,1,2","iiActorObjectMorphNames 1 1\n\"Smirk\" \"Smile\"\n"};
    Fixture()
    {
        topology.points={{{0,0,0},{}},{{1,0,0},{}},{{1,1,0},{}},{{0,1,0},{}}};
        MeshTopologyDefinition d;d.positionCount=4;
        d.layouts={{0,NoTopologyIndex,NoTopologyIndex},{1,NoTopologyIndex,NoTopologyIndex},
                   {2,NoTopologyIndex,NoTopologyIndex},{3,NoTopologyIndex,NoTopologyIndex}};
        d.polygons={{0,1,2},{0,2,3}};d.hiddenPolygons={1};
        topology.layout=MeshTopologyLayout::create(d,topology.points);
        topology.morphTargets.push_back({"Smirk",{}});
        source.faces.count=1;source.num_indices=3;
        name(props[0],"iiActorObjectSourceMesh");props[0].type=UFBX_PROP_INTEGER;props[0].value_int=7;
        name(props[1],"iiActorObjectSourcePointIds");name(props[2],"iiActorObjectSourcePolygonIds");
        name(props[3],"iiActorObjectSourceCornerLayouts");name(props[4],"iiActorObjectSourceMorphs");
        refresh();node.props.props={props.data(),props.size()};
    }
    void refresh()
    {for(std::size_t i=0;i<values.size();++i)text(props[i+1],values[i]);}
    std::optional<ExternalMeshIdentity> read(bool restored=true)
    {return readFbxExternalMeshIdentity(node,source,topology,5,restored,16);}
};
}

int main()
{
    Fixture f;
    auto identity=f.read();
    check(identity&&identity->mesh==5&&identity->source==7);
    check(identity->points==std::vector<std::uint32_t>({0,1,2,3}));
    check(identity->polygons==std::vector<std::uint32_t>({0,1}));
    check(identity->corners==std::vector<std::uint32_t>({0,1,2,0,2,3}));
    check(identity->morphs.size()==1&&identity->morphs[0].morph==0&&identity->morphs[0].source=="Smile");
    auto visible=f.read(false);
    check(visible&&visible->polygons==std::vector<std::uint32_t>({0})&&visible->corners==std::vector<std::uint32_t>({0,1,2}));
    f.values[0]="9,8,7,6";f.refresh();
    check(f.read(false)->points==std::vector<std::uint32_t>({9,8,7,6}));
    rejects([&]{(void)f.read();});
    f.values[0]="0,1,2,4294967295";f.refresh();
    rejects([&]{(void)f.read(false);});
    f.values[0]="0,1,2,3";f.values[2]="0,1,3";f.refresh();
    rejects([&]{(void)f.read();});
    f.values[2]="0,1";f.refresh();
    rejects([&]{(void)f.read(false);});
    f.values[2]="0,1,2";f.values[3]="iiActorObjectMorphNames 1 1\n\"Other\" \"Smile\"\n";f.refresh();
    rejects([&]{(void)f.read();});
    f.values[3]="iiActorObjectMorphNames 1 2\n\"Smirk\" \"Smile\"\n\"Other\" \"Other\"\n";f.refresh();
    rejects([&]{(void)f.read();});
    f.node.props.props={};check(!f.read());

    Fixture reversed;
    auto d=reversed.topology.layout->definition();d.reverseWinding=true;
    reversed.topology.layout=MeshTopologyLayout::create(d,reversed.topology.points);
    reversed.values[2]="2,1,0";reversed.refresh();
    check(reversed.read()->corners==std::vector<std::uint32_t>({0,1,2,0,2,3}));

    ufbx_scene scene{};ufbx_node a{},b{};ufbx_prop docA{},docB{};
    name(docA,"iiActorObjectDocument");name(docB,"iiActorObjectDocument");
    std::string document="project-1",different="project-2";
    text(docA,document);text(docB,document);a.props.props={&docA,1};b.props.props={&docB,1};
    std::array<ufbx_node*,2> nodes{&a,&b};scene.nodes={nodes.data(),nodes.size()};
    check(readFbxExternalDocument(scene)==document);
    text(docB,different);rejects([&]{(void)readFbxExternalDocument(scene);});
    document.clear();text(docA,document);text(docB,document);
    rejects([&]{(void)readFbxExternalDocument(scene);});
    text(docB,document);document=std::string(129,'a');text(docA,document);text(docB,document);
    rejects([&]{(void)readFbxExternalDocument(scene);});
    document=std::string("a\0b",3);text(docA,document);text(docB,document);
    rejects([&]{(void)readFbxExternalDocument(scene);});
    a.props.props={};b.props.props={};check(readFbxExternalDocument(scene).empty());
}
