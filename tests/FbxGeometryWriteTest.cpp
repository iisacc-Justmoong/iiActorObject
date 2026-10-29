#include "../src/Interchange/FBX/Object3DFbxGeometryWrite.h"
#include "../src/Interchange/FBX/Object3DFbxTangentData.h"
#include "../src/Geometry/Object3DMesh.h"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
using namespace iiActorObject::object3d_detail::fbx_binary;
namespace
{
void check(bool value)
{if(!value)throw std::runtime_error("FBX geometry writer contract failed");}
template<class F> void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid FBX geometry was accepted");}
const Node &child(const Node &node,std::string_view name,std::size_t occurrence=0)
{for(const auto &value:node.children)if(value.name==name){if(!occurrence)return value;--occurrence;}throw std::runtime_error("Missing FBX geometry field");}
}
int main()
{
    Object3DMesh mesh;mesh.name="Triangle";mesh.material=7;
    mesh.geometry.vertices={{{0,0,0},{0,0,1},{}},{{1,0,0},{0,0,1},{}},{{0,1,0},{0,0,1},{}}};
    mesh.geometry.triangles={0,1,2};mesh.textureCoordinates={{0,0},{1,0},{0,1}};
    mesh.additionalUvSets.push_back({"Detail",{{0,0},{.5,0},{0,.5}}});
    FbxTangentLayer primary;primary.uvName="UVMap";primary.tangents={{1,0,0},{1,0,0},{1,0,0}};
    primary.handedness={1,1,1};primary.binormals={{0,1,0},{0,1,0},{0,1,0}};
    FbxTangentLayer extra;extra.uvIndex=1;extra.uvName="Detail";extra.binormals={{0,1,0},{0,1,0},{0,1,0}};
    const std::array layers{primary,extra};
    const auto output=writeFbxGeometry(mesh,42,layers);
    check(output.name=="Geometry"&&output.properties[0].bytes==prop(std::int64_t{42}).bytes);
    check(child(output,"Vertices").properties[0].bytes==array(std::array<double,9>{0,0,0,1,0,0,0,1,0}).bytes);
    check(child(output,"PolygonVertexIndex").properties[0].bytes==array(std::array<std::int32_t,3>{0,1,-3}).bytes);
    check(child(child(output,"LayerElementMaterial"),"Materials").properties[0].bytes==array(std::array<std::int32_t,1>{7}).bytes);
    check(child(child(output,"LayerElementUV"),"UVIndex").properties[0].bytes==array(std::array<std::int32_t,3>{0,1,2}).bytes);
    check(child(child(output,"LayerElementUV",1),"UV").properties[0].bytes==array(std::array<double,6>{0,0,.5,0,0,.5}).bytes);
    check(child(child(output,"LayerElementTangent"),"TangentsW").properties[0].bytes==array(std::array<double,3>{1,1,1}).bytes);
    check(child(output,"LayerElementBinormal",1).properties[0].bytes==prop(std::int32_t{1}).bytes);
    check(child(output,"Layer",1).properties[0].bytes==prop(std::int32_t{1}).bytes);

    MeshTopology source;source.points={{{0,0,0},{}},{{1,0,0},{}},{{0,1,0},{}},{{1,1,0},{}}};
    MeshTopologyDefinition definition;definition.positionCount=4;definition.reverseWinding=true;
    definition.layouts={{0,NoTopologyIndex,NoTopologyIndex},{1,NoTopologyIndex,NoTopologyIndex},
                        {2,NoTopologyIndex,NoTopologyIndex},{3,NoTopologyIndex,NoTopologyIndex}};
    definition.polygons={{0,1,2},{1,3,2}};definition.hiddenPolygons={1};definition.polygonMaterials={3,4};
    source.layout=MeshTopologyLayout::create(definition,source.points);
    source.additionalUvSets.push_back({"Detail",{{0,0},{1,0},{0,1},{1,1}},{0,1,2,3}});
    mesh.topology=source;mesh.textureCoordinates.clear();
    auto authored=writeFbxGeometry(mesh,43,{});
    check(child(authored,"Vertices").properties[0].bytes==array(std::array<double,12>{0,0,0,1,0,0,0,1,0,1,1,0}).bytes);
    check(child(authored,"PolygonVertexIndex").properties[0].bytes==array(std::array<std::int32_t,3>{2,1,-1}).bytes);
    check(child(child(authored,"LayerElementNormal"),"NormalsIndex").properties[0].bytes==array(std::array<std::int32_t,3>{2,1,0}).bytes);
    check(child(child(authored,"LayerElementMaterial"),"Materials").properties[0].bytes==array(std::array<std::int32_t,1>{3}).bytes);
    check(child(child(authored,"LayerElementUV"),"UVIndex").properties[0].bytes==array(std::array<std::int32_t,3>{2,1,0}).bytes);
    (void)child(authored,"iiActorObjectTopology");
    auto bad=mesh;bad.geometry.vertices.clear();rejects([&]{(void)writeFbxGeometry(bad,1,{});});
    bad=mesh;bad.topology->additionalUvSets.clear();rejects([&]{(void)writeFbxGeometry(bad,1,{});});
    FbxTangentLayer invalid=primary;invalid.uvIndex=2;
    rejects([&]{(void)writeFbxGeometry(mesh,1,std::span<const FbxTangentLayer>(&invalid,1));});
    invalid=primary;invalid.tangents.pop_back();
    rejects([&]{(void)writeFbxGeometry(mesh,1,std::span<const FbxTangentLayer>(&invalid,1));});
    invalid={};invalid.uvIndex=2;invalid.status="source_tangents_stale";
    const auto warning=writeFbxGeometry(mesh,1,std::span<const FbxTangentLayer>(&invalid,1));
    (void)child(warning,"iiActorObjectTangentStatus");
}
