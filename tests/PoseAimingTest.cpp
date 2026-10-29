#include <iiActorObject/PoseAiming.h>
#include "../src/iiActorObject/CharacterMath.h"
#include <array>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
using namespace iiActorObject;
using namespace iiActorObject::character_detail;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class E=std::exception,class F>void rejects(F f){bool caught=false;try{f();}catch(const E&){caught=true;}CHECK(caught);}
std::shared_ptr<const Skeleton> skeleton(bool scaled=false,bool reflection=false)
{
    auto root=Transform{{.3,.2,.1},Quaternion::fromAxisAngle({0,0,1},.27),scaled?Vector3{1.7,.6,1.2}:Vector3{1,1,1}};
    if(reflection)root.scale.x=-root.scale.x;
    return std::make_shared<Skeleton>(std::vector<Bone>{{"root",NoBone,root},{"endpoint",3,{{.25,-.1,.05},{},{1,1,1}}},
        {"joint",0,{{.4,.8,.1},Quaternion::fromAxisAngle({0,1,0},.31),scaled?Vector3{.8,1.5,1.2}:Vector3{1,1,1}}},
        {"helper",2,{{.3,.2,.1},Quaternion::fromAxisAngle({1,0,0},-.19),{1,1,1}}},{"other",0,{{0,.2,0},{},{1,1,1}}}});
}
void direction(bool scaled=false,bool reflection=false)
{
    const auto source=skeleton(scaled,reflection);
    for(const auto desired:std::array{Vector3{1,0,0},Vector3{-1,0,0},Vector3{0,1,0},Vector3{0,-1,0},Vector3{0,0,1},Vector3{0,0,-1},Vector3{1,2,3},Vector3{1e308,-1e308,1e308}})
    {
        Pose pose(source);const auto revision=pose.revision();const std::vector<Transform> oldLocals(pose.locals().begin(),pose.locals().end());
        const auto oldJoint=pose.worldMatrices()[2].transformPoint({});const auto parentInverse=pose.worldMatrices()[0].invertedAffine();
        const auto oldLength=length(parentInverse.transformVector(sub(pose.worldMatrices()[1].transformPoint({}),oldJoint)));
        const auto result=aimPoseJoint(pose,2,1,desired);CHECK(pose.revision()==revision);CHECK(std::equal(oldLocals.begin(),oldLocals.end(),pose.locals().begin(),pose.locals().end()));
        CHECK(result.position==oldLocals[2].position&&result.scale==oldLocals[2].scale);pose.setLocal(2,result);
        const auto joint=pose.worldMatrices()[2].transformPoint({}),endpoint=pose.worldMatrices()[1].transformPoint({});
        const auto actual=unit(sub(endpoint,joint)),wanted=unit(desired);CHECK(length(cross(actual,wanted))<2e-12&&dot(actual,wanted)>1-2e-12);
        CHECK(length(sub(joint,oldJoint))<1e-12);CHECK(std::abs(length(parentInverse.transformVector(sub(endpoint,joint)))-oldLength)<1e-12);
        CHECK(pose.locals()[3]==oldLocals[3]&&pose.locals()[1]==oldLocals[1]);
    }
}
void opposite()
{
    for(int variant=0;variant<3;++variant)for(const auto offset:{0.,1e-12,1e-9,1e-6})
    {
        Pose pose(skeleton(variant!=0,variant==2));const auto source=unit(sub(pose.worldMatrices()[1].transformPoint({}),pose.worldMatrices()[2].transformPoint({})));
        const auto perpendicular=unit(cross(source,{0,0,1})),wanted=unit(add(mul(source,-1),mul(perpendicular,offset)));
        pose.setLocal(2,aimPoseJoint(pose,2,1,wanted));
        const auto actual=unit(sub(pose.worldMatrices()[1].transformPoint({}),pose.worldMatrices()[2].transformPoint({})));CHECK(length(cross(actual,wanted))<2e-12&&dot(actual,wanted)>1-2e-12);
    }
}
void largeOrigin()
{
    std::optional<Transform> expected;
    for(const auto origin:{0.,1e8,1e12,1e16})
    {
        auto data=std::vector<Bone>{{"root",NoBone,{{origin,origin,origin}}},{"tip",2,{{0,.25,0}}},{"joint",0,{{0,.5,0}}}};
        Pose pose(std::make_shared<Skeleton>(std::move(data)));const auto revision=pose.revision();const auto local=aimPoseJoint(pose,2,1,{1,0,0});
        CHECK(pose.revision()==revision);CHECK(length(sub(local.matrix().transformVector({0,.25,0}),Vector3{.25,0,0}))<1e-13);
        if(expected)CHECK(local==*expected);else expected=local;
    }
}
void dynamicBind()
{
    Pose pose(skeleton());std::vector<Transform> bind;
    for(const auto &bone:pose.skeleton()->bones())bind.push_back(bone.bindTransform);
    bind[2].position.x+=.3;bind[2].rotation=product(bind[2].rotation,Quaternion::fromAxisAngle({0,0,1},.4));
    bind[3].position.y+=.2;pose.setDeformationBind(bind);
    const auto origin=pose.worldMatrices()[2].transformPoint({});
    pose.setLocal(2,aimPoseJoint(pose,2,1,{1,2,3}));
    const auto current=unit(sub(pose.worldMatrices()[1].transformPoint({}),pose.worldMatrices()[2].transformPoint({})));
    CHECK(length(cross(current,unit({1,2,3})))<2e-12);
    CHECK(length(sub(origin,pose.worldMatrices()[2].transformPoint({})))<2e-12);
    bind[2].scale={2,1,1};pose.setDeformationBind(bind);const auto revision=pose.revision();
    rejects<std::invalid_argument>([&]{(void)aimPoseJoint(pose,2,1,{0,1,0});});CHECK(pose.revision()==revision);
    Pose scaled(skeleton(true));bind.clear();for(const auto &bone:scaled.skeleton()->bones())bind.push_back(bone.bindTransform);
    bind[2].rotation=product(bind[2].rotation,Quaternion::fromAxisAngle({0,0,1},.4));scaled.setDeformationBind(bind);
    scaled.setLocal(2,aimPoseJoint(scaled,2,1,{1,2,3}));
    const auto scaledDirection=unit(sub(scaled.worldMatrices()[1].transformPoint({}),scaled.worldMatrices()[2].transformPoint({})));
    CHECK(length(cross(scaledDirection,unit({1,2,3})))<2e-12);
}
void failures()
{
    Pose pose(skeleton());const auto revision=pose.revision();
    rejects<std::out_of_range>([&]{(void)aimPoseJoint(pose,NoBone,1,{1,0,0});});
    rejects<std::out_of_range>([&]{(void)aimPoseJoint(pose,2,99,{1,0,0});});
    rejects<std::invalid_argument>([&]{(void)aimPoseJoint(pose,2,2,{1,0,0});});
    rejects<std::invalid_argument>([&]{(void)aimPoseJoint(pose,2,4,{1,0,0});});
    rejects<std::invalid_argument>([&]{(void)aimPoseJoint(pose,2,1,{});});
    rejects<std::invalid_argument>([&]{(void)aimPoseJoint(pose,2,1,{std::numeric_limits<double>::infinity(),0,0});});
    rejects<std::length_error>([&]{(void)aimPoseJoint(pose,2,1,{1,0,0},PoseAimOptions{1});});
    rejects<std::invalid_argument>([&]{(void)aimPoseJoint(pose,2,1,{1,0,0},PoseAimOptions{0});});
    CHECK(pose.revision()==revision);
    auto bones=std::vector<Bone>(pose.skeleton()->bones().begin(),pose.skeleton()->bones().end());bones[1].bindTransform.position={};bones[3].bindTransform.position={};
    Pose collapsed(std::make_shared<Skeleton>(std::move(bones)));rejects<std::invalid_argument>([&]{(void)aimPoseJoint(collapsed,2,1,{1,0,0});});
}
void group()
{
    for(int variant=0;variant<3;++variant)
    {
        const auto base=skeleton(variant!=0,variant==2);
        auto bones=std::vector<Bone>(base->bones().begin(),base->bones().end());
        bones.push_back({"second",2,{{.1,.35,-.2}}});
        Pose pose(std::make_shared<Skeleton>(std::move(bones)));
        const auto revision=pose.revision();const PoseJointAim aim{2,1,{1,2,3},{5}};
        auto transform=aimPoseJoint(pose,aim);CHECK(pose.revision()==revision);pose.setLocal(2,transform);
        const auto world=pose.worldMatrices();const auto point=world[2].transformPoint({});
        const auto center=mul(add(world[1].transformPoint({}),world[5].transformPoint({})),.5);
        const auto current=unit(sub(center,point)),target=unit(aim.objectDirection);
        CHECK(length(cross(current,target))<2e-12&&dot(current,target)>1-2e-12);
        auto bad=aim;bad.additionalDescendants={1};rejects([&]{(void)aimPoseJoint(pose,bad);});
        bad.additionalDescendants={4};rejects([&]{(void)aimPoseJoint(pose,bad);});
        bad.additionalDescendants={99};rejects([&]{(void)aimPoseJoint(pose,bad);});
        bad.additionalDescendants.assign(256,5);rejects([&]{(void)aimPoseJoint(pose,bad);});
        rejects([&]{(void)aimPoseJoint(pose,aim,PoseAimOptions{1});});
    }
    std::optional<Transform> atOrigin;
    for(const auto origin:{0.,1e8,1e16})
    {
        auto bones=std::vector<Bone>{{"root",NoBone,{{origin,origin,origin}}},{"joint",0,{{0,.5,0}}},
            {"toe-a",1,{{.1,.3,.2}}},{"toe-b",1,{{-.1,.3,.2}}}};
        Pose pose(std::make_shared<Skeleton>(std::move(bones)));const PoseJointAim aim{1,2,{1,0,0},{3}};
        const auto value=aimPoseJoint(pose,aim);if(atOrigin)CHECK(value==*atOrigin);else atOrigin=value;
    }
    Pose zero(std::make_shared<Skeleton>(std::vector<Bone>{{"root",NoBone,{}},{"joint",0,{}},
        {"a",1,{{0,1,0}}},{"b",1,{{0,-1,0}}}}));
    rejects([&]{(void)aimPoseJoint(zero,PoseJointAim{1,2,{1,0,0},{3}});});
}
int main(int argc,char **argv)
{
    try{if(argc!=2)return 2;const std::string_view mode=argv[1];if(mode=="direction")direction();else if(mode=="scaled")direction(true);else if(mode=="reflection")direction(true,true);else if(mode=="opposite")opposite();else if(mode=="large_origin")largeOrigin();else if(mode=="dynamic_bind")dynamicBind();else if(mode=="failures")failures();else if(mode=="group")group();else return 2;std::cout<<mode<<" passed\n";return 0;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
