#include "../src/Interchange/GLB/Object3DGlbSharedMeshWrite.h"
#include "../src/Interchange/GLB/Object3DGlbBuffer.h"
#include <iiActorObject/Object3DMesh.h>
#include <stdexcept>
#include <utility>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail::glb_export;
namespace
{
void check(bool v){if(!v)throw std::runtime_error("GLB shared mesh writer contract failed");}
template<class F>void rejects(F f){try{f();}catch(const std::exception &){return;}throw std::runtime_error("Invalid shared mesh accepted");}
}
int main()
{
    Object3DMesh mesh;mesh.geometry.vertices={{{0,0,0},{0,0,1},{{0,1}}},{{1,0,0},{0,0,1},{{0,1}}},{{0,1,0},{0,0,1},{{0,1}}}};
    mesh.geometry.triangles={0,1,2};mesh.textureCoordinates={{0,0},{1,0},{0,1}};
    mesh.additionalUvSets={{"Detail",{{0,1},{1,1},{0,0}}}};
    mesh.geometry.morphTargets={{"Smile",{{1,{0,.1,0},{0,.2,0}}}}};
    MeshTopology topology;topology.points={{{0,0,0},{{0,1}}},{{1,0,0},{{0,1}}},{{0,1,0},{{0,1}}}};
    topology.normals={{0,0,1}};topology.textureCoordinates=mesh.textureCoordinates;
    MeshTopologyDefinition definition;definition.positionCount=3;definition.normalCount=1;definition.textureCoordinateCount=3;
    definition.layouts={{0,0,0},{1,1,0},{2,2,0}};definition.polygons={{0,1,2}};
    topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);mesh.topology=std::move(topology);
    Buffer buffer;const auto [attributes,targets]=writeGlbSharedMeshData(buffer,mesh,true,1);
    check(attributes==R"({"POSITION":0,"NORMAL":1,"JOINTS_0":2,"WEIGHTS_0":3,"TEXCOORD_0":4,"TEXCOORD_1":5,"_IIACTOROBJECT_POINT_ID":6})");
    check(targets==R"(,"targets":[{"POSITION":7,"NORMAL":8}])"&&buffer.accessors.size()==9);
    auto noRig=mesh;noRig.geometry.morphTargets.clear();noRig.topology.reset();
    Buffer plain;const auto [plainAttributes,plainTargets]=writeGlbSharedMeshData(plain,noRig,false,0);
    check(plainAttributes.find("JOINTS_")==std::string::npos&&plainTargets.empty());
    auto invalid=mesh;invalid.textureCoordinates.pop_back();Buffer untouched;
    rejects([&]{(void)writeGlbSharedMeshData(untouched,invalid,true,1);});
    invalid=mesh;invalid.additionalUvSets[0].coordinates.pop_back();
    rejects([&]{(void)writeGlbSharedMeshData(untouched,invalid,true,1);});
    invalid=mesh;invalid.geometry.morphTargets[0].deltas[0].vertex=99;
    rejects([&]{(void)writeGlbSharedMeshData(untouched,invalid,true,1);});
    check(untouched.bytes.empty()&&untouched.views.empty()&&untouched.accessors.empty());
}
