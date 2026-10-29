#include "../src/Interchange/FBX/Object3DFbxAnimationStage.h"
#include <array>
#include <stdexcept>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool value){if(!value)throw std::runtime_error("FBX animation stage contract failed");}
template<class F>void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid FBX animation accepted");}
FbxImportedClip clip()
{
    FbxImportedClip result{"Take",2,1,{},{{"Smile",{{0,0},{1,1}}}},{{0,1,0}}};
    return result;
}
}
int main()
{
    auto skeleton=std::make_shared<Skeleton>(std::vector<Bone>{{"root",NoBone,{}}});
    std::vector<Object3DMesh> meshes(2);meshes[1].geometry.morphTargets.push_back({"Smile",{}});
    const std::vector<Bone> bones{{"root",NoBone,{}}};
    const std::vector<FbxImportedClip> valid{clip()};
    validateFbxAnimationClips(bones,valid);
    validateFbxAnimationClips({},valid); // Static assets still validate against a synthetic root.
    const auto built=stageFbxAnimationClips(skeleton,meshes,valid);
    check(built.size()==1&&built[0].name=="Take"&&built[0].sourceStartSeconds==2&&
        built[0].clip->skeleton()==skeleton&&built[0].clip->sampleCurve(0,.5)==.5&&
        built[0].morphBindings[0].mesh==1);
    for(unsigned field=0;field!=3;++field)
    {
        auto bad=valid;
        if(field==0)bad[0].morphBindings[0].curve=1;
        if(field==1)bad[0].morphBindings[0].mesh=2;
        if(field==2)bad[0].morphBindings[0].target=1;
        rejects([&]{(void)stageFbxAnimationClips(skeleton,meshes,bad);});
    }
    auto invalid=valid;invalid[0].duration=0;
    rejects([&]{validateFbxAnimationClips(bones,invalid);});
    auto second=valid;second.push_back(clip());second[1].morphBindings[0].target=2;
    rejects([&]{(void)stageFbxAnimationClips(skeleton,meshes,second);});
    check(built.size()==1&&built[0].clip->skeleton()==skeleton);
}
