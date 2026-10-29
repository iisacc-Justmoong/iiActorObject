#include "../src/Interchange/GLB/Object3DGlbPrimitiveTopology.h"
#include <iiActorObject/MeshTopologyCore.h>
#include <array>
#include <limits>
#include <stdexcept>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail::glb_import;
namespace
{
void check(bool value){if(!value)throw std::runtime_error("GLB primitive topology contract failed");}
template<class F>void rejects(F f){try{f();}catch(const std::exception &){return;}throw std::runtime_error("Invalid GLB primitive topology accepted");}
}
int main()
{
    std::vector<TopologyControlPoint> points={{{0,0,0},{{0,1}}},{{1,0,0},{{0,1}}},
                                               {{0,1,0},{{0,1}}},{{1,1,0},{{0,1}}}};
    MeshTopologyDefinition definition;definition.layouts={{0},{1},{0},{1},{2},{3}};
    std::vector<std::uint8_t> missingNormals;
    const std::array<std::uint32_t,6> indices{0,1,2,2,1,3};
    appendGlbPrimitiveTriangles(indices,2,4,points,true,7,true,definition,missingNormals);
    check(definition.polygons==std::vector<std::vector<std::uint32_t>>({{2,3,4},{4,3,5}}));
    check(definition.polygonMaterials==std::vector<std::uint32_t>({7,7})&&missingNormals==std::vector<std::uint8_t>({0,0}));
    const std::array<std::uint32_t,3> other{0,1,2};
    appendGlbPrimitiveTriangles(other,2,4,points,true,3,false,definition,missingNormals);
    check(definition.polygons.size()==3&&definition.polygonMaterials.back()==3&&missingNormals.back()==1);

    const auto saved=definition;const auto savedMissing=missingNormals;
    const std::array<std::uint32_t,6> invalid{0,1,2,0,1,4};
    rejects([&]{appendGlbPrimitiveTriangles(invalid,2,4,points,true,7,true,definition,missingNormals);});
    check(definition==saved&&missingNormals==savedMissing);
    rejects([&]{appendGlbPrimitiveTriangles(std::span(indices).first(2),2,4,points,true,7,true,definition,missingNormals);});
    rejects([&]{appendGlbPrimitiveTriangles(indices,3,4,points,true,7,true,definition,missingNormals);});
    rejects([&]{appendGlbPrimitiveTriangles(indices,2,4,points,true,std::numeric_limits<std::uint32_t>::max(),true,definition,missingNormals);});
    auto unweighted=points;unweighted[3].weights.clear();
    rejects([&]{appendGlbPrimitiveTriangles(indices,2,4,unweighted,true,7,true,definition,missingNormals);});
    auto broken=definition;broken.layouts[2].position=9;
    rejects([&]{appendGlbPrimitiveTriangles(indices,2,4,points,false,7,true,broken,missingNormals);});
    auto mismatched=missingNormals;mismatched.pop_back();
    rejects([&]{appendGlbPrimitiveTriangles(indices,2,4,points,true,7,true,definition,mismatched);});
    check(definition==saved&&missingNormals==savedMissing);
}
