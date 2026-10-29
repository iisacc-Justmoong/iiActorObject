#include "../src/Interchange/FBX/Object3DFbxGeometryRegistry.h"
#include "../src/Interchange/FBX/Object3DFbxSharedGeometry.h"
#include "../src/Geometry/Object3DMesh.h"
#include <stdexcept>
#include <vector>
using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
using namespace iiActorObject::object3d_detail::fbx_binary;
namespace
{
void check(bool value){if(!value)throw std::runtime_error("FBX geometry registry contract failed");}
template<class E,class F>void rejects(F action)
{try{action();}catch(const E &){return;}throw std::runtime_error("Invalid FBX geometry registration accepted or wrong error");}
Node geometry(std::int64_t id,int count)
{return {"Geometry",{prop(id)},{value("Count",prop(count))}};}
}
int main()
{
    std::vector<Object3DMesh> meshes(3);for(auto &mesh:meshes)mesh.geometry.vertices.resize(2);
    Node objects{"Objects",{},{Node{"Earlier",{},{}}}};std::size_t work=0;FbxGeometryRegistry registry(3);
    check(registry.stage(0,0,101,geometry(101,2),meshes,objects,work)==101&&objects.children.size()==2);
    check(registry.stage(1,0,102,geometry(102,2),meshes,objects,work)==101&&objects.children.size()==2&&work>0);
    rejects<SharedProgressiveGeometryMismatch>([&]{(void)registry.stage(2,0,103,geometry(103,3),meshes,objects,work);});
    check(objects.children.size()==2);
    check(registry.stage(2,2,103,geometry(103,3),meshes,objects,work)==103&&objects.children.size()==3);
    Node otherObjects{"Objects",{},{}};
    rejects<std::invalid_argument>([&]{(void)registry.stage(1,0,104,geometry(104,2),meshes,otherObjects,work);});
    rejects<std::invalid_argument>([&]{(void)registry.stage(0,0,104,geometry(104,2),meshes,objects,work);});
    rejects<std::invalid_argument>([&]{(void)registry.stage(3,0,104,geometry(104,2),meshes,objects,work);});
    rejects<std::invalid_argument>([&]{(void)registry.stage(1,3,104,geometry(104,2),meshes,objects,work);});
    FbxGeometryRegistry pending(3);Node pendingObjects{"Objects",{},{}},unfinished{"Objects",{},{}};std::size_t pendingWork=0;
    rejects<SharedProgressiveGeometryMismatch>([&]{(void)pending.stage(0,1,104,geometry(104,2),meshes,pendingObjects,pendingWork);});
    check(pendingObjects.children.empty());
    rejects<std::invalid_argument>([&]{(void)pending.stage(0,0,104,geometry(104,2),std::span<const Object3DMesh>{meshes}.first(2),unfinished,pendingWork);});
    FbxGeometryRegistry limited(3);Node limitedObjects{"Objects",{}, {}};std::size_t limitWork=0;
    check(limited.stage(0,0,105,geometry(105,2),meshes,limitedObjects,limitWork)==105);
    limitWork=128'000'000;
    rejects<std::length_error>([&]{(void)limited.stage(1,0,106,geometry(106,2),meshes,limitedObjects,limitWork);});
    check(limitedObjects.children.size()==1);
    objects.children.clear();
    rejects<std::invalid_argument>([&]{(void)registry.stage(1,0,107,geometry(107,2),meshes,objects,work);});
}
