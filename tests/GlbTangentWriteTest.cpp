#include "../src/Interchange/GLB/Object3DGlbTangentWrite.h"
#include "../src/Interchange/GLB/Object3DGlbTangentData.h"
#include "../src/Interchange/GLB/Object3DGlbPointAttributeWrite.h"
#include "../src/Interchange/GLB/Object3DGlbBuffer.h"
#include <iiActorObject/Object3DMesh.h>
#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
using namespace iiActorObject::object3d_detail::glb_export;
namespace
{
void check(bool v){if(!v)throw std::runtime_error("GLB tangent writer contract failed");}
template<class F>void rejects(F f){try{f();}catch(const std::exception &){return;}throw std::runtime_error("Invalid GLB tangent writer input accepted");}
}
int main()
{
    Object3DMesh mesh;
    mesh.geometry.vertices={{{0,0,0},{0,0,1},{{0,1}}},{{1,0,0},{0,0,1},{{0,1}}},{{0,1,0},{0,0,1},{{0,1}}}};
    mesh.geometry.triangles={0,1,2};mesh.textureCoordinates={{0,0},{1,0},{0,1}};
    mesh.geometry.morphTargets={{"Smile",{{1,{0,.1,0},{0,.2,0}}}}};
    GlbTangentPlan plan;plan.sourceVertices={2,0,1};plan.tangents={{{1,0,0},1},{{1,0,0},1},{{1,0,0},1}};
    plan.morphTangents={{"",{{2,{}, {0,.5,0}}}}};
    const std::array<std::string_view,1> notes{"note\"one"};Buffer buffer;
    const auto primitive=writeGlbTangentPrimitive(buffer,mesh,plan,true,1,3,"authored",notes,1);
    check(primitive.find(R"("POSITION":0,"NORMAL":1,"TANGENT":2,"JOINTS_0":3,"WEIGHTS_0":4,"TEXCOORD_0":5)")!=primitive.npos);
    check(primitive.find(R"("material":3,"targets":[{"POSITION":6,"NORMAL":7,"TANGENT":8}])")!=primitive.npos);
    check(primitive.find(R"("iiActorObjectAuthoredTangentStatus":"authored")")!=primitive.npos);
    check(primitive.find(R"("iiActorObjectFbxTangentNotes":["note\"one"],"iiActorObjectFbxTangentUvSet":1)")!=primitive.npos);
    check(buffer.accessors.size()==10);
    auto extras=stream();writeGlbTangentExtras(extras,"fallback",{}, {},0);
    check(extras.str()==R"(,"extras":{"iiActorObjectTangentFallback":"fallback","iiActorObjectFbxTangentUvSet":0})");

    MeshTopology topology;topology.points={{{0,0,0},{}},{{1,0,0},{}},{{0,1,0},{}}};
    topology.normals={{0,0,1}};topology.textureCoordinates=mesh.textureCoordinates;
    MeshTopologyDefinition definition;definition.positionCount=3;definition.normalCount=1;definition.textureCoordinateCount=3;
    definition.layouts={{0,0,0},{1,1,0},{2,2,0}};definition.polygons={{0,1,2}};
    topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);mesh.topology=std::move(topology);
    Buffer identity;auto attributes=stream();
    writeGlbPointIdentityAttribute(identity,mesh,3,plan.sourceVertices,attributes);
    check(identity.bytes==std::vector<std::uint8_t>({2,0,0,0,0,0,0,0,1,0,0,0}));
    check(attributes.str()==R"(,"_IIACTOROBJECT_POINT_ID":0)");
    Buffer linear;auto linearAttributes=stream();
    writeGlbPointIdentityAttribute(linear,mesh,3,{},linearAttributes);
    check(linear.bytes==std::vector<std::uint8_t>({0,0,0,0,1,0,0,0,2,0,0,0}));
    auto invalid=plan;invalid.tangents.pop_back();Buffer untouched;
    rejects([&]{(void)writeGlbTangentPrimitive(untouched,mesh,invalid,true,1,0);});
    invalid=plan;invalid.sourceVertices[1]=99;
    rejects([&]{(void)writeGlbTangentPrimitive(untouched,mesh,invalid,true,1,0);});
    invalid=plan;invalid.morphTangents.clear();
    rejects([&]{(void)writeGlbTangentPrimitive(untouched,mesh,invalid,true,1,0);});
    auto invalidUv=mesh;invalidUv.textureCoordinates.pop_back();
    rejects([&]{(void)writeGlbTangentPrimitive(untouched,invalidUv,plan,true,1,0);});
    auto invalidMorph=mesh;invalidMorph.geometry.morphTargets[0].deltas[0].vertex=99;
    rejects([&]{(void)writeGlbTangentPrimitive(untouched,invalidMorph,plan,true,1,0);});
    auto shortMap=std::span(plan.sourceVertices).first(2);auto unused=stream();
    rejects([&]{writeGlbPointIdentityAttribute(untouched,mesh,3,shortMap,unused);});
    check(untouched.bytes.empty()&&untouched.views.empty()&&untouched.accessors.empty()&&unused.str().empty());
}
