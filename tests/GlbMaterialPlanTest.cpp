#include "../src/Interchange/GLB/Object3DGlbMaterialPlan.h"
#include <iiActorObject/Object3DMaterial.h>
#include <iiActorObject/Object3DMesh.h>
#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail::glb_export;
namespace
{
void check(bool value){if(!value)throw std::runtime_error("GLB material plan contract failed");}
template<class F> void rejects(F f){try{f();}catch(const std::exception &){return;}throw std::runtime_error("Invalid GLB material plan accepted");}
}
int main()
{
    Object3DMesh mesh;mesh.additionalUvSets={{"Paint",{}},{"Normal",{}}};
    check(glbMaterialUvIndex(mesh,"")==0&&glbMaterialUvIndex(mesh,"UVMap")==0);
    check(glbMaterialUvIndex(mesh,"Paint")==1&&glbMaterialUvIndex(mesh,"Normal")==2);
    mesh.primaryUvName="Surface";check(glbMaterialUvIndex(mesh,"Surface")==0);
    rejects([&]{(void)glbMaterialUvIndex(mesh,"UVMap");});
    Object3DMaterial first,second;first.baseColorUvSet="Paint";first.normalUvSet="Normal";
    std::vector<Object3DMaterial> materials{first,second};
    GlbMaterialPlan plan(materials);
    check(plan.materials().data()==materials.data()&&plan.materials().size()==materials.size());
    check(plan.variants().size()==2&&plan.variants()[0]==GlbMaterialVariant({0,0,0})&&
          plan.variants()[1]==GlbMaterialVariant({1,0,0}));
    check(plan.materialFor(mesh,0)==2&&plan.variants()[2]==GlbMaterialVariant({0,1,2}));
    check(plan.materialFor(mesh,0)==2&&plan.materialFor(mesh,1)==1&&plan.variants().size()==3);
    auto reordered=mesh;std::swap(reordered.additionalUvSets[0],reordered.additionalUvSets[1]);
    check(plan.materialFor(reordered,0)==3&&plan.variants()[3]==GlbMaterialVariant({0,2,1}));
    const std::vector<GlbMaterialVariant> saved(plan.variants().begin(),plan.variants().end());
    rejects([&]{(void)plan.materialFor(mesh,2);});
    auto missing=mesh;missing.additionalUvSets.pop_back();
    rejects([&]{(void)plan.materialFor(missing,0);});
    check(std::vector<GlbMaterialVariant>(plan.variants().begin(),plan.variants().end())==saved);
}
