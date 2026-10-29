#include "../src/Interchange/GLB/Object3DGlbFlatNormals.h"
#include <iiActorObject/MeshTopologyCore.h>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail::glb_import;
namespace
{
void check(bool value)
{if(!value)throw std::runtime_error("GLB flat normals contract failed");}
template<class F>void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid flat normal input was accepted");}
}
int main()
{
    MeshTopology topology;topology.points={{{0,0,0},{}},{{1,0,0},{}},{{0,1,0},{}},{{1,1,0},{}}};
    topology.normals={{0,0,1}};
    topology.additionalUvSets.push_back({"detail",{{0,0},{1,0},{0,1},{1,1}},{0,1,2,1,3,2}});
    topology.morphTargets.push_back({"rise",{{0,{0,0,1}}},NoTopologyIndex,std::vector<TopologyNormalDelta>{}});
    MeshTopologyDefinition definition;definition.positionCount=4;
    definition.layouts={{0},{1},{2},{1,NoTopologyIndex,0},{3,NoTopologyIndex,0},{2,NoTopologyIndex,0}};
    definition.polygons={{0,1,2},{3,4,5}};
    std::size_t flatWork=0;
    const std::array<std::uint8_t,2> missing{1,0};
    rejects([&]{(void)generateGlbFlatNormals(topology,definition,flatWork,std::array<std::uint8_t,2>{2,0});});
    auto invalidDefinition=definition;invalidDefinition.polygons[0][0]=99;
    rejects([&]{(void)generateGlbFlatNormals(topology,invalidDefinition,flatWork,missing);});
    check(flatWork==0&&topology.normals.size()==1);
    const auto generated=generateGlbFlatNormals(topology,definition,flatWork,missing);
    check(generated==3&&flatWork==3&&definition.layouts.size()==9&&topology.normals.size()==3);
    check(definition.polygons[0]==std::vector<std::uint32_t>({6,7,8})&&definition.polygons[1]==std::vector<std::uint32_t>({3,4,5}));
    check(topology.additionalUvSets[0].layoutIndices==std::vector<std::uint32_t>({0,1,2,1,3,2,0,1,2}));
    check(std::abs(topology.normals.back().z-1)<1e-12);
    definition.normalCount=static_cast<std::uint32_t>(topology.normals.size());
    topology.layout=MeshTopologyLayout::create(definition,topology.points);
    std::size_t morphWork=0;
    generateGlbFlatMorphNormals(topology,missing,generated,100,morphWork);
    check(morphWork==10&&topology.morphTargets[0].normalDeltas->size()==3);
    for(const auto &delta:*topology.morphTargets[0].normalDeltas)
        check(delta.layout>=6&&delta.normal.x>0&&delta.normal.y>0&&delta.normal.z<0);

    rejects([&]{generateGlbFlatMorphNormals(topology,std::array<std::uint8_t,1>{1},generated,100,morphWork);});
    std::size_t exhausted=100;
    rejects([&]{generateGlbFlatMorphNormals(topology,missing,generated,100,exhausted);});
    rejects([&]{(void)generateGlbFlatNormals(topology,definition,flatWork,std::array<std::uint8_t,1>{1});});
}
