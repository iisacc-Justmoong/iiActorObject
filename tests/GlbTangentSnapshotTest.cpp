#include "../src/Interchange/GLB/Object3DGlbTangentSnapshot.h"
#include <iiActorObject/MeshTopologyCore.h>
#include <array>
#include <stdexcept>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail::glb_import;
namespace
{
void check(bool value){if(!value)throw std::runtime_error("GLB tangent snapshot contract failed");}
template<class F>void rejects(F f){try{f();}catch(const std::exception &){return;}throw std::runtime_error("Invalid GLB tangent snapshot accepted");}
}
int main()
{
    MeshTopology topology;topology.points={{{0,0,0},{}},{{1,0,0},{}},{{0,1,0},{}}};
    topology.normals={{0,0,1}};topology.textureCoordinates={{0,0},{1,0},{0,1}};topology.primaryUvName="UVMap";
    MeshTopologyDefinition definition;definition.positionCount=3;definition.normalCount=1;definition.textureCoordinateCount=3;
    definition.layouts={{0,0,0},{1,1,0},{2,2,0}};definition.polygons={{0,1,2}};definition.polygonMaterials={2};
    topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);
    topology.morphTargets={{"Face",{{0,{0,0,.1}}},0}};
    TopologyGlbTangentSource input;input.tangents={{0,{1,0,0},1}};input.morphTangents={{{0,{0,1,0}}}};
    const std::array bindings{TopologyGlbTangentBinding{2,true,"UVMap"}};
    const auto snapshot=makeGlbTangentSnapshot(topology,input,bindings);
    check(snapshot&&snapshot->definition==topology.layout->definition()&&snapshot->triangles.size()==3);
    check(snapshot->positions==std::vector<Vector3>({{0,0,0},{1,0,0},{0,1,0}}));
    check(snapshot->normals==topology.normals&&snapshot->textureCoordinates==topology.textureCoordinates&&snapshot->primaryUvName=="UVMap");
    check(snapshot->morphTargets==topology.morphTargets&&snapshot->morphTangents.size()==1&&snapshot->tangents.size()==1);
    check(snapshot->bindings.size()==1&&snapshot->bindings[0].material==2&&snapshot->bindings[0].normalMapped&&snapshot->bindings[0].normalUvSet=="UVMap");
    check(!topology.sourceGlbTangents&&input.positions.empty()&&input.bindings.empty());
    auto missing=topology;missing.layout.reset();rejects([&]{(void)makeGlbTangentSnapshot(missing,input,bindings);});
    TopologyGlbTangentSource empty;check(!makeGlbTangentSnapshot(missing,empty,bindings));
}
