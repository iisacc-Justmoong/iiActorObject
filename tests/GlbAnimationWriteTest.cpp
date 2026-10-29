#include "../src/Interchange/GLB/Object3DGlbAnimationWrite.h"
#include "../src/Interchange/GLB/Object3DGlbBuffer.h"
#include <iiActorObject/Object3DData.h>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail::glb_export;
namespace
{
void check(bool value)
{if(!value)throw std::runtime_error("GLB animation writer contract failed");}
template<class F>void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid GLB animation was accepted");}
float valueAt(const std::vector<std::uint8_t> &bytes,std::size_t offset)
{
    std::uint32_t raw=0;for(unsigned i=0;i<4;++i)raw|=std::uint32_t(bytes.at(offset+i))<<(8*i);
    return std::bit_cast<float>(raw);
}
}
int main()
{
    auto skeleton=std::make_shared<Skeleton>(std::vector<Bone>{{"root",NoBone,{}}});
    AnimationClip clip(skeleton,1,{},{{"Smile",{{0,0},{1,1}}}});
    Object3DData data;Object3DMesh mesh;mesh.name="Face";
    mesh.geometry.morphTargets={{"Smile",{}},{"Blink",{}}};mesh.morphDefaults={{"Blink",.25}};
    data.meshes.push_back(mesh);
    GlbAnimationExportOptions options;GlbMorphAnimationOptions morphOptions;morphOptions.matchUnmappedCurvesByName=true;
    check(checkGlbAnimation(skeleton.get(),false,clip,options,true)==0);
    auto plans=planGlbMorphAnimation(data,clip,options,morphOptions,0);
    check(plans.size()==1&&plans[0].mesh==0&&plans[0].times==std::vector<double>({0,1}));
    check(plans[0].values.size()==16&&valueAt(plans[0].values,0)==0&&valueAt(plans[0].values,4)==.25f&&
          valueAt(plans[0].values,8)==1&&valueAt(plans[0].values,12)==.25f);
    Buffer buffer;const auto json=writeGlbAnimation(buffer,clip,0,plans);
    check(json.find(R"("node":1,"path":"weights")")!=std::string::npos&&buffer.accessors.size()==2);

    const std::array<GlbMorphCurveBinding,2> duplicates{{{"Smile","Smile",{}},{"Smile","Smile",{}}}};
    morphOptions.bindings=duplicates;rejects([&]{(void)planGlbMorphAnimation(data,clip,options,morphOptions,0);});
    morphOptions.bindings={};rejects([&]{(void)planGlbMorphAnimation(data,clip,options,morphOptions,options.maximumKeys+1);});
    morphOptions.maximumWeightValues=1;rejects([&]{(void)planGlbMorphAnimation(data,clip,options,morphOptions,0);});
    rejects([&]{(void)checkGlbAnimation(skeleton.get(),false,clip,options,false);});
    auto other=std::make_shared<Skeleton>(std::vector<Bone>{{"root",NoBone,{}}});
    rejects([&]{(void)checkGlbAnimation(other.get(),false,clip,options,true);});

    BoneTrack track{0,{{.25,{}},{.75,{}}}};AnimationClip bones(skeleton,1,{track});
    check(checkGlbAnimation(skeleton.get(),true,bones,options,false)==4);
    std::vector<MorphAnimationPlan> noMorphs;Buffer boneBuffer;
    const auto boneJson=writeGlbAnimation(boneBuffer,bones,1,noMorphs);
    check(boneJson.find(R"("node":0,"path":"translation")")!=std::string::npos&&boneBuffer.accessors.size()==4);
    BoneTrack collision{0,{{.5,{}},{std::nextafter(.5,1.),{}}}};AnimationClip dense(skeleton,1,{collision});
    rejects([&]{(void)checkGlbAnimation(skeleton.get(),true,dense,options,false);});
}
