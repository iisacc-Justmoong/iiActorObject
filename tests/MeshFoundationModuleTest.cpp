#include <iiActorObject/MeshTopologyCore.h>
#include <stdexcept>

int main()
{
    using namespace iiActorObject;
    MeshTopology topology;
    topology.points={{{0,0,0},{}},{{1,0,0},{}},{{0,1,0},{}},{{2,2,2},{}}};
    MeshTopologyDefinition definition;
    definition.sourceFormat="isolated";definition.positionCount=4;
    definition.layouts={{0},{1},{2}};definition.polygons={{0,1,2}};
    topology.layout=MeshTopologyLayout::create(definition,topology.points);
    auto render=renderMeshTopology(topology);
    if(topology.points.size()!=4||render.geometry.vertices.size()!=3||render.geometry.triangles.size()!=3)
        return 1;
    if(render.geometry.vertices[1].position!=topology.points[1].position)return 2;
    auto bad=definition;bad.polygons[0][0]=99;
    try {(void)MeshTopologyLayout::create(bad,topology.points);}
    catch(const std::exception &){return 0;}
    return 3;
}
