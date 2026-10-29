#include "../src/Interchange/GLB/Object3DGlbTangentPlan.h"
#include <iiActorObject/Object3DMesh.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool v){if(!v)throw std::runtime_error("GLB tangent plan contract failed");}
template<class F>void rejects(F f){try{f();}catch(const std::exception &){return;}throw std::runtime_error("Invalid GLB tangent plan accepted");}
}
int main()
{
    Object3DMesh mesh;
    mesh.geometry.vertices={{{0,0,0},{0,0,1},{}},{{1,0,0},{0,0,1},{}},
                            {{0,1,0},{0,0,1},{}},{{-1,0,0},{0,0,1},{}}};
    mesh.geometry.triangles={0,1,2,0,2,3};
    mesh.textureCoordinates={{0,0},{1,0},{0,1},{1,0}};
    const std::array<std::uint32_t,3> corners{2,0,1};
    std::string_view reason="old";
    const auto generated=planGlbTangents(mesh,corners,mesh.textureCoordinates,reason);
    check(generated&&reason.empty()&&generated->sourceVertices==std::vector<std::uint32_t>({2,0,1}));
    check(generated->tangents.size()==3&&std::abs(generated->tangents[0].direction.x)>.99);
    check(!planGlbTangents(mesh,{},mesh.textureCoordinates,reason)&&reason=="corner_budget");
    const std::array<std::uint32_t,1> badCorner{99};
    check(!planGlbTangents(mesh,badCorner,mesh.textureCoordinates,reason)&&reason=="invalid_basis");
    check(!planGlbTangents(mesh,corners,std::span(mesh.textureCoordinates).first(2),reason)&&reason=="invalid_basis");

    const std::array<std::uint32_t,6> allCorners{0,1,2,3,4,5};
    const std::array<std::uint32_t,6> toPolygon{0,1,2,3,4,5};
    TopologyTangentSourceLayer layer;layer.tangents.assign(6,{1,0,0});
    layer.binormals={{0,1,0},{0,1,0},{0,1,0},{},{},{}};
    const std::array<std::string_view,1> notes{"source_note"};
    const auto split=planFbxGlbTangents(mesh,layer,allCorners,toPolygon,notes);
    check(split.authored.sourceVertices==std::vector<std::uint32_t>({0,1,2}));
    check(split.authored.tangents.size()==3&&split.authored.tangents[0].handedness==-1);
    check(split.fallbackCorners==std::vector<std::uint32_t>({3,4,5}));
    check(std::find(split.fallbackNotes.begin(),split.fallbackNotes.end(),"source_fbx_handedness_unknown")!=split.fallbackNotes.end());
    check(split.authoredNotes[0]=="source_note");
    layer.binormals.clear();
    const auto missing=planFbxGlbTangents(mesh,layer,allCorners,toPolygon,{});
    check(missing.authored.sourceVertices.empty()&&missing.fallbackCorners.size()==6);

    MeshTopology topology;topology.points={{{0,0,0},{}},{{1,0,0},{}},{{0,1,0},{}}};
    topology.normals={{0,0,1}};topology.textureCoordinates={{0,0},{1,0},{0,1}};
    MeshTopologyDefinition definition;definition.positionCount=3;definition.normalCount=1;definition.textureCoordinateCount=3;
    definition.layouts={{0,0,0},{1,1,0},{2,2,0}};definition.polygons={{0,1,2}};
    topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);
    auto source=std::make_shared<TopologyGlbTangentSource>();
    source->tangents={{0,{1,0,0},1},{1,{1,0,0},1},{2,{1,0,0},1}};
    source->morphTangents={{{1,{0,.5,0}}}};topology.sourceGlbTangents=source;
    mesh.topology=std::move(topology);mesh.geometry.triangles={0,1,2};
    mesh.geometry.morphTargets={{"Smile",{{1,{0,.1,0},{}}}}};
    const std::array<std::uint32_t,3> byLayout{0,1,2};std::size_t work=0;
    const auto authored=planAuthoredGlbTangents(mesh,corners,byLayout,work);
    check(work==4&&authored.sourceVertices==std::vector<std::uint32_t>({2,0,1}));
    check(authored.morphTangents.size()==1&&authored.morphTangents[0].deltas.size()==1);
    check(authored.morphTangents[0].deltas[0].vertex==2&&authored.morphTangents[0].deltas[0].normal.y==.5);
    auto missingTopology=mesh;missingTopology.topology.reset();
    rejects([&]{(void)planAuthoredGlbTangents(missingTopology,corners,byLayout,work);});
    const std::array<std::uint32_t,2> wrong{0,1};
    rejects([&]{(void)planAuthoredGlbTangents(mesh,corners,wrong,work);});
    work=256'000'001;
    rejects([&]{(void)planAuthoredGlbTangents(mesh,corners,byLayout,work);});
}
