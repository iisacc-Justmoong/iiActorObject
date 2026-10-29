#include "../src/Interchange/FBX/Object3DFbxTangentProjection.h"
#include <iiActorObject/Object3DMesh.h>
#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool v){if(!v)throw std::runtime_error("FBX tangent projection contract failed");}
}
int main()
{
    Object3DMesh mesh;mesh.geometry.triangles={0,1,2};std::string_view reason;
    const auto plain=projectFbxTangentCorners(mesh,reason);
    check(plain&&plain->triangleToPolygon==std::vector<std::uint32_t>({0,1,2})&&plain->cornerCount==3);
    mesh.geometry.triangles.push_back(0);
    check(!projectFbxTangentCorners(mesh,reason)&&reason=="polygon_mapping");
    mesh.geometry.triangles.clear();check(!projectFbxTangentCorners(mesh,reason)&&reason=="corner_budget");
    MeshTopology topology;topology.points={{{0,0,0},{}},{{1,0,0},{}},{{1,1,0},{}},{{0,1,0},{}}};
    MeshTopologyDefinition definition;definition.positionCount=4;definition.layouts={{0},{1},{2},{3}};
    definition.polygons={{0,1,2,3}};definition.reverseWinding=true;
    topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);
    mesh.geometry.triangles.assign(topology.layout->triangles().begin(),topology.layout->triangles().end());mesh.topology=topology;
    const auto quad=projectFbxTangentCorners(mesh,reason);
    check(quad&&quad->cornerCount==4&&quad->triangleToPolygon.size()==6);
    auto sorted=quad->triangleToPolygon;std::sort(sorted.begin(),sorted.end());
    sorted.erase(std::unique(sorted.begin(),sorted.end()),sorted.end());
    check(sorted==std::vector<std::uint32_t>({0,1,2,3}));
    auto hiddenTopology=topology;auto hiddenDefinition=topology.layout->definition();
    hiddenDefinition.polygons={{0,1,2},{0,2,3}};hiddenDefinition.hiddenPolygons={1};
    hiddenDefinition.renderToLayout.clear();
    hiddenTopology.layout=MeshTopologyLayout::create(std::move(hiddenDefinition),hiddenTopology.points);
    mesh.topology=hiddenTopology;
    mesh.geometry.triangles.assign(hiddenTopology.layout->triangles().begin(),hiddenTopology.layout->triangles().end());
    const auto visible=projectFbxTangentCorners(mesh,reason);
    check(visible&&visible->cornerCount==3&&visible->triangleToPolygon.size()==3);
    mesh.topology=topology;mesh.geometry.triangles.assign(topology.layout->triangles().begin(),topology.layout->triangles().end());
    mesh.topology->layout.reset();check(!projectFbxTangentCorners(mesh,reason)&&reason=="polygon_mapping");
    mesh.topology=topology;mesh.geometry.triangles[0]=99;
    check(!projectFbxTangentCorners(mesh,reason)&&reason=="polygon_mapping");
}
