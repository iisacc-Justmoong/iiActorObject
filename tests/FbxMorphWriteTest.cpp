#include "../src/Interchange/FBX/Object3DFbxMorphWrite.h"
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
using namespace iiActorObject::object3d_detail::fbx_binary;
namespace
{
void check(bool value)
{if(!value)throw std::runtime_error("FBX morph writer contract failed");}
template<class F> void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid FBX morph writer input was accepted");}
const Node &child(const Node &node,std::string_view name)
{for(const auto &value:node.children)if(value.name==name)return value;throw std::runtime_error("Missing FBX shape field");}
}
int main()
{
    MeshTopology source;
    source.points={{{0,0,0},{}},{{1,0,0},{}},{{0,1,0},{}}};source.normals={{0,0,2}};
    MeshTopologyDefinition definition;definition.positionCount=3;definition.normalCount=1;
    definition.layouts={{0,NoTopologyIndex,0},{1,NoTopologyIndex,0},{2,NoTopologyIndex,0},{1,NoTopologyIndex,0}};
    definition.polygons={{0,1,2}};definition.renderToLayout={0,1,2};
    source.layout=MeshTopologyLayout::create(definition,source.points);
    TopologyMorph morph;morph.name="Smile";morph.deltas={{0,{1,2,3}}};
    morph.normalDeltas=std::vector<TopologyNormalDelta>{{1,{.2,0,0}},{3,{.5,0,0}}};
    std::size_t work=0;
    const auto encoded=encodeFbxMorph(morph,&source,source.points.size(),true,false,work);
    check(!encoded.exactNormals&&encoded.explicitNormals&&encoded.indices==std::vector<std::int32_t>({0,1}));
    check(encoded.deltas==std::vector<double>({1,2,3,0,0,0}));
    check(encoded.normalDeltas==std::vector<double>({0,0,0,.4,0,0})&&encoded.normalMetadata.size()==56&&work==0);
    const auto node=makeFbxMorphShape(encoded,"Smile",99);
    check(node.name=="Geometry"&&node.properties[0].bytes==prop(std::int64_t{99}).bytes);
    check(child(node,"Indexes").properties[0].bytes==array(std::array<std::int32_t,2>{0,1}).bytes);
    check(child(node,"Vertices").properties[0].bytes==array(std::array<double,6>{1,2,3,0,0,0}).bytes);
    check(child(node,"Normals").properties[0].bytes==array(std::array<double,6>{0,0,0,.4,0,0}).bytes);
    auto malformed=encoded;malformed.deltas.pop_back();
    rejects([&]{(void)makeFbxMorphShape(malformed,"Bad",1);});
    malformed=encoded;malformed.indices[0]=-1;
    rejects([&]{(void)makeFbxMorphShape(malformed,"Bad",1);});
    rejects([&]{std::size_t budget=0;(void)encodeFbxMorph(morph,&source,3,false,false,budget);});
    rejects([&]{std::size_t budget=0;(void)encodeFbxMorph(morph,&source,3,true,true,budget);});
    auto exact=morph;exact.normalDeltas->at(1).normal={.2,0,0};
    const auto progressive=encodeFbxMorph(exact,&source,3,false,true,work);
    check(progressive.exactNormals&&progressive.normalMetadata.empty()&&work==7);
    auto broken=exact;broken.normalDeltas->at(0).layout=9;
    rejects([&]{std::size_t budget=0;(void)encodeFbxMorph(broken,&source,3,true,false,budget);});
    broken=exact;broken.deltas.push_back(broken.deltas[0]);
    rejects([&]{std::size_t budget=0;(void)encodeFbxMorph(broken,&source,3,true,false,budget);});
    broken=exact;broken.deltas[0].position.x=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{std::size_t budget=0;(void)encodeFbxMorph(broken,&source,3,true,false,budget);});
    rejects([&]{std::size_t budget=128'000'000;(void)encodeFbxMorph(exact,&source,3,true,true,budget);});
    TopologyMorph simple;simple.name="Plain";simple.deltas={{2,{0,0,1}}};
    std::size_t budget=0;auto simpleData=encodeFbxMorph(simple,nullptr,3,false,false,budget);
    check(!simpleData.explicitNormals&&simpleData.indices==std::vector<std::int32_t>{2});
    check(makeFbxMorphShape(simpleData,"Plain",100).children.size()==3);
    simple.normalDeltas=std::vector<TopologyNormalDelta>{};
    check(encodeFbxMorph(simple,nullptr,3,false,false,budget).explicitNormals);
}
