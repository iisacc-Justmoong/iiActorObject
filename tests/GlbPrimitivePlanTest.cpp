#include "../src/Interchange/GLB/Object3DGlbPrimitivePlan.h"
#include "../src/Interchange/GLB/Object3DGlbTangentSnapshot.h"
#include <iiActorObject/Object3DMaterial.h>
#include <iiActorObject/Object3DMesh.h>
#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail::glb_export;
using iiActorObject::object3d_detail::glb_import::makeGlbTangentSnapshot;
namespace
{
void check(bool v){if(!v)throw std::runtime_error("GLB primitive partition contract failed");}
template<class F>void rejects(F f){try{f();}catch(const std::exception &){return;}throw std::runtime_error("Invalid GLB primitive plan accepted");}
}
int main()
{
    Object3DMesh mesh;mesh.geometry.vertices={{{0,0,0},{0,0,1},{}},{{1,0,0},{0,0,1},{}},
                                             {{1,1,0},{0,0,1},{}},{{0,1,0},{0,0,1},{}}};
    mesh.geometry.triangles={0,1,2,0,2,3};mesh.triangleMaterials={1,0};
    std::vector<Object3DMaterial> materials(2);
    auto plain=planGlbPrimitives(mesh,materials);
    check(plain.parts.size()==2&&plain.tangentByLayout.empty());
    check(plain.parts.at({0,false}).corners==std::vector<std::uint32_t>({3,4,5}));
    check(plain.parts.at({1,false}).vertices==std::vector<std::uint32_t>({0,1,2}));
    auto invalid=mesh;invalid.geometry.triangles.pop_back();rejects([&]{(void)planGlbPrimitives(invalid,materials);});
    invalid=mesh;invalid.triangleMaterials.pop_back();rejects([&]{(void)planGlbPrimitives(invalid,materials);});
    invalid=mesh;invalid.geometry.triangles[0]=99;rejects([&]{(void)planGlbPrimitives(invalid,materials);});
    invalid=mesh;invalid.triangleMaterials[0]=2;rejects([&]{(void)planGlbPrimitives(invalid,materials);});

    MeshTopology topology;topology.points={{{0,0,0},{}},{{1,0,0},{}},{{1,1,0},{}},{{0,1,0},{}}};
    topology.normals={{0,0,1}};topology.textureCoordinates={{0,0},{1,0},{1,1},{0,1}};
    MeshTopologyDefinition definition;definition.positionCount=4;definition.normalCount=1;definition.textureCoordinateCount=4;
    definition.layouts={{0,0,0},{1,1,0},{2,2,0},{3,3,0}};definition.polygons={{0,1,2},{0,2,3}};
    topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);
    mesh.geometry.triangles.assign(topology.layout->triangles().begin(),topology.layout->triangles().end());
    mesh.triangleMaterials={0,1};TopologyGlbTangentSource authored;
    authored.tangents={{0,{1,0,0},1},{1,{1,0,0},1},{2,{1,0,0},1},{3,{1,0,0},1}};
    const std::array bindings{TopologyGlbTangentBinding{0,false,{}},TopologyGlbTangentBinding{1,false,{}}};
    topology.sourceGlbTangents=makeGlbTangentSnapshot(topology,authored,bindings);mesh.topology=topology;
    materials[1].normalImage=0;materials[1].normalUvSet="Detail";
    auto partition=planGlbPrimitives(mesh,materials);
    check(partition.parts.size()==2&&partition.tangentByLayout==std::vector<std::uint32_t>({0,1,2,3}));
    check(partition.parts.contains({0,true})&&partition.parts.contains({1,false}));
    check(partition.parts.at({1,false}).authoredStatus=="source_tangent_uv_changed");
    mesh.topology->points[0].position.z=.25;
    auto stale=planGlbPrimitives(mesh,materials);
    check(stale.tangentByLayout.empty()&&stale.parts.at({0,false}).authoredStatus=="source_tangents_stale");
    mesh.topology=topology;authored.tangents.resize(2);
    mesh.topology->sourceGlbTangents=makeGlbTangentSnapshot(*mesh.topology,authored,bindings);
    rejects([&]{(void)planGlbPrimitives(mesh,materials);});
}
