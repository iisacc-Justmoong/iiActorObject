#include "../src/Interchange/GLB/Object3DGlbMeshExtrasWrite.h"
#include "../src/Interchange/GLB/Object3DGlbPointIdentity.h"
#include "../src/Interchange/GLB/Object3DGlbBuffer.h"
#include <iiActorObject/Object3DMesh.h>
#include <iiActorObject/Skeleton.h>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
using namespace iiActorObject::object3d_detail::glb_export;
namespace
{
void check(bool v){if(!v)throw std::runtime_error("GLB mesh extras writer contract failed");}
template<class F>void rejects(F f){try{f();}catch(const std::exception &){return;}throw std::runtime_error("Invalid GLB mesh extras accepted");}
}
int main()
{
    Object3DMesh mesh;mesh.geometry.morphTargets={{"Smile",{}},{"Blink",{}}};mesh.morphDefaults={{"Blink",.25}};
    MeshTopology topology;topology.points={{{0,0,0},{}},{{1,0,0},{{0,1}}},{{0,1,0},{{0,1}}},{{3,4,5},{{0,1}}}};
    MeshTopologyDefinition definition;definition.positionCount=4;definition.layouts={{0},{1},{2}};definition.polygons={{0,1,2}};
    topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);
    topology.morphTargets={{"Smile",{{3,{0,0,.25}}},0}};mesh.topology=std::move(topology);
    const std::array bones{Bone{"root",NoBone,{}}};Buffer buffer;auto out=stream();
    writeGlbMeshExtras(out,buffer,mesh,bones,true,4);
    check(out.str()==R"(,"weights":[0,0.25],"extras":{"targetNames":["Smile","Blink"],"iiActorObjectPoints":{"version":1,"bufferView":0},"iiActorObjectSourceMesh":4})");
    const auto decoded=decodeGlbPointIdentity(buffer.bytes,4,2,1,1,1);
    check(decoded.pointCount==4&&decoded.targetCount==2&&decoded.unused.size()==1&&decoded.unused[0].id==3);
    check(decoded.boneNames==std::vector<std::string>{"root"}&&decoded.morphs[0].delta.z==.25);
    mesh.topology.reset();Buffer plain;auto plainOut=stream();writeGlbMeshExtras(plainOut,plain,mesh,{},false,0);
    check(plainOut.str()==R"(,"weights":[0,0.25],"extras":{"targetNames":["Smile","Blink"]})"&&plain.bytes.empty());
    mesh.morphDefaults={{"Smile",std::numeric_limits<double>::denorm_min()}};
    rejects([&]{Buffer failed;auto result=stream();writeGlbMeshExtras(result,failed,mesh,{},false,0);});
    mesh.geometry.morphTargets.clear();auto empty=stream();writeGlbMeshExtras(empty,plain,mesh,{},false,0);check(empty.str().empty());
}
