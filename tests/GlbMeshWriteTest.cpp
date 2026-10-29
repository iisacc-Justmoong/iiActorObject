#include "../src/Interchange/GLB/Object3DGlbMeshWrite.h"
#include "../src/Interchange/GLB/Object3DGlbBuffer.h"
#include "../src/Interchange/GLB/Object3DGlbMaterialPlan.h"
#include <iiActorObject/Object3DMaterial.h>
#include <iiActorObject/Object3DMesh.h>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail::glb_export;
namespace
{
void check(bool v){if(!v)throw std::runtime_error("GLB mesh writer contract failed");}
template<class F>void rejects(F f){try{f();}catch(const std::exception &){return;}throw std::runtime_error("Invalid GLB mesh writer input accepted");}
}
int main()
{
    Object3DMesh mesh;mesh.name="Body";
    mesh.geometry.vertices={{{0,0,0},{0,0,1},{}},{{1,0,0},{0,0,1},{}},{{0,1,0},{0,0,1},{}}};
    mesh.geometry.triangles={0,1,2};mesh.textureCoordinates={{0,0},{1,0},{0,1}};
    std::vector<Object3DMaterial> materials(1);GlbMaterialPlan plan(materials);Buffer buffer;
    const auto plain=writeGlbMesh(buffer,mesh,plan,{},false,false,0);
    check(plain==R"({"name":"Body","primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3,"material":0}]})");
    check(buffer.accessors.size()==4);
    Object3DMaterial mapped;mapped.normalImage=0;
    std::vector<Object3DMaterial> mappedMaterials{mapped};GlbMaterialPlan mappedPlan(mappedMaterials);Buffer mappedBuffer;
    const auto normal=writeGlbMesh(mappedBuffer,mesh,mappedPlan,{},false,false,0);
    check(normal.find(R"("POSITION":0,"NORMAL":1,"TANGENT":2,"TEXCOORD_0":3)")!=normal.npos);
    check(normal.find(R"("indices":4,"material":0)")!=normal.npos&&mappedBuffer.accessors.size()==5);
    auto multiple=mesh;multiple.geometry.triangles={0,1,2,0,2,1};multiple.triangleMaterials={1,0};
    std::vector<Object3DMaterial> twoMaterials(2);GlbMaterialPlan twoPlan(twoMaterials);Buffer shared;
    const auto parts=writeGlbMesh(shared,multiple,twoPlan,{},false,false,0);
    check(parts.find(R"("indices":3,"material":0)")!=parts.npos&&parts.find(R"("indices":4,"material":1)")!=parts.npos);
    check(shared.accessors.size()==5);
    Buffer untouched;auto bad=mesh;bad.geometry.triangles[0]=9;GlbMaterialPlan badPlan(materials);
    rejects([&]{(void)writeGlbMesh(untouched,bad,badPlan,{},false,false,0);});
    rejects([&]{(void)writeGlbMesh(untouched,mesh,badPlan,{},false,true,0);});
    check(untouched.bytes.empty()&&untouched.views.empty()&&untouched.accessors.empty());
    MeshTopology topology;topology.points={{{0,0,0},{}},{{1,0,0},{}},{{0,1,0},{}}};
    topology.normals={{0,0,1}};topology.textureCoordinates=mesh.textureCoordinates;
    MeshTopologyDefinition definition;definition.positionCount=3;definition.normalCount=1;definition.textureCoordinateCount=3;
    definition.layouts={{0,0,0},{1,1,0},{2,2,0}};definition.polygons={{0,1,2}};
    topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);mesh.topology=std::move(topology);
    GlbMaterialPlan externalPlan(materials);Buffer externalBuffer;
    const auto external=writeGlbMesh(externalBuffer,mesh,externalPlan,{},false,true,7);
    check(external.find("_IIACTOROBJECT_POINT_ID")!=external.npos&&
          external.find(R"("iiActorObjectSourceMesh":7)")!=external.npos&&
          external.find("iiActorObjectPoints")!=external.npos);
}
