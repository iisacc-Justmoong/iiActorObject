#include <iiActorObject/PoseFrames.h>
#include "../src/iiActorObject/CharacterMath.h"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace iiActorObject;
using namespace iiActorObject::character_detail;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class E=std::exception,class F>void rejects(F f){bool caught=false;try{f();}catch(const E&){caught=true;}CHECK(caught);}
std::shared_ptr<const Skeleton> skeleton(bool scaled=false,bool reflected=false,bool localReflection=false,bool largeOrigin=false)
{
    const auto rootPosition=largeOrigin?Vector3{1e16,-1e16,1e16}:Vector3{.3,.2,.1};
    std::vector<Bone> bones{{"root",NoBone,{rootPosition,Quaternion::fromAxisAngle({0,0,1},.27),scaled?Vector3{1.7,.6,1.2}:Vector3{1,1,1}}},
        {"primary",3,{{.4,.1,.02},{},{1,1,1}}},
        {"joint",5,{{.4,.8,.1},Quaternion::fromAxisAngle({1,0,0},.21),scaled?Vector3{.8,1.5,1.2}:Vector3{1,1,1}}},
        {"helper",2,{{.3,.2,.1},Quaternion::fromAxisAngle({0,0,1},-.3),scaled?Vector3{1.2,.7,1}:Vector3{1,1,1}}},
        {"plane",3,{{.2,.4,.25},{},{1,1,1}}},
        {"parent",0,{{.1,.2,.3},Quaternion::fromAxisAngle({0,1,0},.37),scaled?Vector3{1.1,.9,1.3}:Vector3{1,1,1}}},
        {"other",0,{{0,.2,0},{},{1,1,1}}}};
    if(reflected)bones[0].bindTransform.scale.x*=-1;if(localReflection)bones[2].bindTransform.scale.z*=-1;
    return std::make_shared<Skeleton>(std::move(bones));
}
void near(Vector3 a,Vector3 b){CHECK(length(sub(a,b))<2e-10);}
void run(bool scaled=false,bool reflected=false,bool localReflection=false,bool rootJoint=false)
{
    for(const auto pair:std::array{std::array{Vector3{1,0,0},Vector3{0,1,0}},std::array{Vector3{1,0,0},Vector3{0,0,1}},std::array{Vector3{-1,0,0},Vector3{0,-1,0}},std::array{Vector3{0,1,0},Vector3{0,0,-1}},std::array{Vector3{0,0,1},Vector3{1,0,0}},std::array{Vector3{1,2,3},Vector3{2,-1,0}}})
    {
        Pose pose(skeleton(scaled,reflected,localReflection));const BoneIndex joint=rootJoint?0:2;
        const auto revision=pose.revision();const std::vector<Transform> locals(pose.locals().begin(),pose.locals().end());
        const auto world=pose.worldMatrices();const auto origin=world[joint].transformPoint({});Matrix4 parentInverse;
        if(!rootJoint)parentInverse=world[5].invertedAffine();
        const auto a=parentInverse.transformVector(sub(world[1].transformPoint({}),origin)),b=parentInverse.transformVector(sub(world[4].transformPoint({}),origin));
        const auto result=orientPoseJointFrame(pose,{joint,1,4,pair[0],pair[1]});CHECK(pose.revision()==revision);
        CHECK(std::equal(locals.begin(),locals.end(),pose.locals().begin(),pose.locals().end()));CHECK(result.position==locals[joint].position&&result.scale==locals[joint].scale);
        pose.setLocal(joint,result);const auto after=pose.worldMatrices();const auto changedOrigin=after[joint].transformPoint({});
        const auto first=sub(after[1].transformPoint({}),changedOrigin),second=sub(after[4].transformPoint({}),changedOrigin);
        near(unit(first),unit(pair[0]));near(unit(cross(unit(first),unit(second))),unit(pair[1]));near(changedOrigin,origin);
        CHECK(std::abs(length(parentInverse.transformVector(first))-length(a))<2e-10);CHECK(std::abs(length(parentInverse.transformVector(second))-length(b))<2e-10);
        for(std::size_t i=0;i<locals.size();++i)if(i!=joint)CHECK(pose.locals()[i]==locals[i]);
    }
}
void largeOrigin()
{
    Pose small(skeleton(true,true,true)),large(skeleton(true,true,true,true));
    const PoseJointFrame frame{2,1,4,{1,0,0},{0,-1,0}};const auto a=orientPoseJointFrame(small,frame),b=orientPoseJointFrame(large,frame);
    const auto x=a.rotation.normalized(),y=b.rotation.normalized();CHECK(std::abs(std::abs(x.x*y.x+x.y*y.y+x.z*y.z+x.w*y.w)-1)<1e-12);
}
void dynamicBind()
{
    Pose pose(skeleton());std::vector<Transform> bind;
    for(const auto &bone:pose.skeleton()->bones())bind.push_back(bone.bindTransform);
    bind[2].rotation=product(bind[2].rotation,Quaternion::fromAxisAngle({0,1,0},.3));
    bind[2].position.x+=.2;bind[3].position.y+=.1;pose.setDeformationBind(bind);
    const auto origin=pose.worldMatrices()[2].transformPoint({});
    pose.setLocal(2,orientPoseJointFrame(pose,{2,1,4,{1,0,0},{0,0,1}}));
    const auto after=pose.worldMatrices();const auto first=sub(after[1].transformPoint({}),after[2].transformPoint({}));
    const auto second=sub(after[4].transformPoint({}),after[2].transformPoint({}));
    near(unit(first),{1,0,0});near(unit(cross(unit(first),unit(second))),{0,0,1});
    near(origin,after[2].transformPoint({}));
    bind[2].scale={2,1,1};pose.setDeformationBind(bind);const auto revision=pose.revision();
    rejects<std::invalid_argument>([&]{(void)orientPoseJointFrame(pose,{2,1,4,{0,1,0},{0,0,1}});});CHECK(pose.revision()==revision);
    Pose scaled(skeleton(true));bind.clear();for(const auto &bone:scaled.skeleton()->bones())bind.push_back(bone.bindTransform);
    bind[2].rotation=product(bind[2].rotation,Quaternion::fromAxisAngle({0,1,0},.3));scaled.setDeformationBind(bind);
    scaled.setLocal(2,orientPoseJointFrame(scaled,{2,1,4,{1,0,0},{0,0,1}}));
    const auto worlds=scaled.worldMatrices();const auto a=sub(worlds[1].transformPoint({}),worlds[2].transformPoint({}));
    const auto b=sub(worlds[4].transformPoint({}),worlds[2].transformPoint({}));
    near(unit(a),{1,0,0});near(unit(cross(unit(a),unit(b))),{0,0,1});
}
void failures()
{
    Pose pose(skeleton(true));const auto revision=pose.revision();const PoseJointFrame good{2,1,4,{1,0,0},{0,1,0}};
    auto frame=good;frame.joint=NoBone;rejects<std::out_of_range>([&]{(void)orientPoseJointFrame(pose,frame);});
    frame=good;frame.planeDescendant=99;rejects<std::out_of_range>([&]{(void)orientPoseJointFrame(pose,frame);});
    frame=good;frame.primaryDescendant=2;rejects<std::invalid_argument>([&]{(void)orientPoseJointFrame(pose,frame);});
    frame=good;frame.planeDescendant=1;rejects<std::invalid_argument>([&]{(void)orientPoseJointFrame(pose,frame);});
    frame=good;frame.planeDescendant=6;rejects<std::invalid_argument>([&]{(void)orientPoseJointFrame(pose,frame);});
    frame=good;frame.objectDirection={};rejects<std::invalid_argument>([&]{(void)orientPoseJointFrame(pose,frame);});
    frame=good;frame.objectPlaneNormal={std::numeric_limits<double>::infinity(),0,0};rejects<std::invalid_argument>([&]{(void)orientPoseJointFrame(pose,frame);});
    frame=good;frame.objectPlaneNormal={1,1,0};rejects<std::invalid_argument>([&]{(void)orientPoseJointFrame(pose,frame);});
    PoseFrameOptions options;options.maximumAncestorVisits=3;rejects<std::length_error>([&]{(void)orientPoseJointFrame(pose,good,options);});
    options.maximumAncestorVisits=0;rejects<std::invalid_argument>([&]{(void)orientPoseJointFrame(pose,good,options);});
    options={};options.minimumPlaneSine=0;rejects<std::invalid_argument>([&]{(void)orientPoseJointFrame(pose,good,options);});
    options={};options.orthogonalityTolerance=-1;rejects<std::invalid_argument>([&]{(void)orientPoseJointFrame(pose,good,options);});
    CHECK(pose.revision()==revision);
    auto bones=std::vector<Bone>(pose.skeleton()->bones().begin(),pose.skeleton()->bones().end());bones[3].bindTransform.position={};bones[4].bindTransform.position=mul(bones[1].bindTransform.position,2);Pose collinear(std::make_shared<Skeleton>(std::move(bones)));
    rejects<std::invalid_argument>([&]{(void)orientPoseJointFrame(collinear,good);});
}
int main(int argc,char **argv)
{
    try{if(argc!=2)return 2;const std::string_view mode=argv[1];if(mode=="basic")run();else if(mode=="scaled")run(true);else if(mode=="reflection")run(true,true);else if(mode=="local_reflection")run(true,false,true);else if(mode=="both_reflections")run(true,true,true);else if(mode=="root")run(true,true,true,true);else if(mode=="large_origin")largeOrigin();else if(mode=="dynamic_bind")dynamicBind();else if(mode=="failures")failures();else return 2;std::cout<<mode<<" passed\n";return 0;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
