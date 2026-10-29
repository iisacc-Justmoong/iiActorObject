#include "../src/Interchange/GLB/Object3DGlbAccessorRead.h"
#include "GlbAccessorFixture.h"
#include <iiActorObject/GlbDocument.h>
#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail::glb_import;
namespace
{
void check(bool value){if(!value)throw std::runtime_error("GLB accessor reader contract failed");}
template<class F>void rejects(F f){try{f();}catch(const std::exception &){return;}throw std::runtime_error("Invalid GLB accessor accepted");}

}
int main()
{
    const auto doc=glb_test::fixture();
    check(readGlbFloatAttribute(doc,0,GlbAccessorType::Vec3,2,false,GlbFloatRole::Position)==std::vector<double>({0,0,0,1,2,3}));
    check(readGlbFloatAttribute(doc,1,GlbAccessorType::Vec3,2,true,GlbFloatRole::Normal)[2]==1);
    check(readGlbFloatAttribute(doc,1,GlbAccessorType::Vec3,2,true,GlbFloatRole::MorphPosition)[2]==1);
    check(readGlbFloatAttribute(doc,2,GlbAccessorType::Vec4,2,true,GlbFloatRole::Tangent)[3]==1);
    rejects([&]{(void)readGlbFloatAttribute(doc,1,GlbAccessorType::Vec3,2,false,GlbFloatRole::Normal);});
    rejects([&]{(void)readGlbFloatAttribute(doc,3,GlbAccessorType::Vec2,2,true,GlbFloatRole::MorphPosition);});
    rejects([&]{(void)readGlbFloatAttribute(doc,0,GlbAccessorType::Vec3,3,false,GlbFloatRole::Position);});
    const auto uv=readGlbTexcoords(doc,3,2,false);check(uv.size()==4&&uv[1]==1&&uv[2]>0.5&&uv[2]<0.51);
    rejects([&]{(void)readGlbTexcoords(doc,4,2,false);});
    const std::array<std::size_t,1> joints{4},weights{5};
    const auto skin=readGlbSkinAttributes(doc,joints,weights,2,true);
    check(skin.joints.size()==1&&skin.joints[0].size()==8&&skin.joints[0][1]==1&&skin.weights[0][0]==1);
    rejects([&]{(void)readGlbSkinAttributes(doc,joints,weights,2,false);});
    rejects([&]{(void)readGlbSkinAttributes(doc,joints,{},2,true);});
    rejects([&]{(void)readGlbSkinAttributes(doc,{},weights,2,true);});
    rejects([&]{(void)readGlbSkinAttributes(doc,weights,weights,2,true);});
    rejects([&]{(void)readGlbSkinAttributes(doc,joints,joints,2,true);});
    const auto point=readGlbPointIds(doc,6,2);check(!point.invalid&&point.values==std::vector<std::uint32_t>({0,0,1,0}));
    check(readGlbPointIds(doc,1,2).invalid&&readGlbPointIds(doc,100,2).invalid);
    check(readGlbTriangleIndices(doc,7,2,1,0)==std::vector<std::uint32_t>({0,1,0}));
    check(readGlbTriangleIndices(doc,std::nullopt,6,2,0)==std::vector<std::uint32_t>({0,1,2,3,4,5}));
    rejects([&]{(void)readGlbTriangleIndices(doc,7,2,1,1);});
    rejects([&]{(void)readGlbTriangleIndices(doc,7,2,1,2);});
    rejects([&]{(void)readGlbTriangleIndices(doc,std::nullopt,2,3,0);});
    rejects([&]{(void)readGlbTriangleIndices(doc,1,2,3,0);});
}
