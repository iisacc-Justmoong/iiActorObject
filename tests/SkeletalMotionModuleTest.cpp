#include <iiActorObject/AnimationRetarget.h>
#include <iiActorObject/CharacterRig.h>
#include <iiActorObject/PoseReference.h>
#include <iiActorObject/RigSampleCatalogCore.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>
#include <stdexcept>

namespace
{
bool near(double a,double b){return std::abs(a-b)<1e-9;}
void check(bool condition){if(!condition)throw std::runtime_error("Skeletal motion isolation check failed");}
}

int main()
{
    using namespace iiActorObject;
    // Bone IDs remain stable even if their parents occur later in the array.
    auto skeleton=std::make_shared<Skeleton>(std::vector<Bone>{
        {"root",NoBone,{}}, {"tip",2,{{0,1,0},{},{1,1,1}}}, {"parent",0,{{1,0,0},{},{1,1,1}}}});
    const std::array<BoneIndex,3> expectedOrder{0,2,1};
    const auto order=skeleton->evaluationOrder();
    check(order.size()==expectedOrder.size()&&std::equal(order.begin(),order.end(),expectedOrder.begin()));
    auto pose=std::make_shared<Pose>(skeleton);
    auto initial=pose->worldMatrices()[1].transformPoint({});
    check(near(initial.x,1)&&near(initial.y,1));

    auto start=skeleton->bones()[1].bindTransform,finish=start;
    finish.position.y=3;
    finish.rotation=Quaternion::fromAxisAngle({0,0,1},std::numbers::pi/2);
    AnimationClip clip(skeleton,2,{{1,{{0,start},{2,finish}}}},{{"amount",{{0,0},{2,1}}}});
    clip.sample(1,*pose);
    check(near(pose->locals()[1].position.y,2)&&near(pose->locals()[1].rotation.z,std::sin(std::numbers::pi/8)));
    check(near(clip.sampleCurve(0,1),.5));

    Pose reference(skeleton);
    const std::array<PoseRetargetJoint,3> mapping{{{0,0},{1,1},{2,2}}};
    const auto retargeted=retargetPose(reference,*pose,reference,mapping);
    check(near(retargeted[1].rotation.z,pose->locals()[1].rotation.z));
    check(near(retargeted[1].position.y,reference.locals()[1].position.y));
    const std::array<double,3> times{0,1,2};
    const auto baked=retargetAnimation(clip,reference,reference,mapping,times);
    Pose result(skeleton);baked.sample(1,result);
    check(near(result.locals()[1].rotation.z,std::sin(std::numbers::pi/8)));
    const std::array<JointControl,1> controls{{{1,start,1}}};
    CharacterRig::apply(result,controls);
    check(near(result.locals()[1].rotation.z,0));

    RigSampleCatalog catalog;
    catalog.sourceFormat="isolation";catalog.lodCount=1;
    catalog.sourceBones.assign(skeleton->bones().begin(),skeleton->bones().end());
    catalog.joints={"root","tip","parent"};catalog.jointMapping={0,1,2};
    catalog.rawControls={"smile"};catalog.baseline.controls={0};
    for(const auto &bone:catalog.sourceBones)catalog.baseline.sourceLocals.push_back(bone.bindTransform);
    auto sample=catalog.baseline;sample.controls[0]=1;
    catalog.samples={{"smileMorph",sample}};
    validateRigSampleCatalogs(std::span(&catalog,1));
    catalog.samples[0].morph.clear();
    try {validateRigSampleCatalogs(std::span(&catalog,1));}
    catch(const std::invalid_argument &){return 0;}
    return 1;
}
