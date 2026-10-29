#include <iiActorObject/Animation.h>
#include <iiActorObject/PoseRetarget.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
Quaternion q(Vector3 axis,double radians){return Quaternion::fromAxisAngle(axis,radians);}
Quaternion multiply(Quaternion a,Quaternion b)
{
    return Quaternion{a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z}.normalized();
}
Matrix4 rotation(Quaternion value){Transform transform;transform.rotation=value;return transform.matrix();}
void near(Vector3 a,Vector3 b){CHECK(std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)<2e-10);}
std::shared_ptr<Skeleton> source()
{
    return std::make_shared<Skeleton>(std::vector<Bone>{{"root",NoBone,{{1,2,3}}},{"tip",2,{{1,0,0},q({1,0,0},.2)}},
        {"arm",0,{{0,1,0},q({0,1,0},-.3)}}});
}
std::shared_ptr<Skeleton> target()
{
    return std::make_shared<Skeleton>(std::vector<Bone>{{"root",NoBone,{{5,6,7},q({0,1,0},.4)}},
        {"tip",3,{{2,0,0},q({0,1,0},.25)}},{"arm",0,{{0,2,0},q({1,0,0},.7)}},
        {"helper",2,{{.3,0,0},q({0,0,1},.2)}}});
}
const std::array pairs{PoseRetargetJoint{2,2},PoseRetargetJoint{1,1},PoseRetargetJoint{0,0}};
void compareWorlds(const Pose &sourceReference,const Pose &sourcePose,const Pose &targetReference,
    const Pose &targetPose,std::span<const PoseRetargetJoint> mapping,Quaternion basis={})
{
    const auto sr=sourceReference.worldMatrices(),sp=sourcePose.worldMatrices(),tr=targetReference.worldMatrices(),tp=targetPose.worldMatrices();
    const auto conversion=rotation(basis),inverse=conversion.invertedAffine();
    for(const auto &pair:mapping)
    {
        const auto expected=conversion*sp[pair.source]*sr[pair.source].invertedAffine()*inverse*tr[pair.target];
        for(const Vector3 axis:std::array{Vector3{1,0,0},Vector3{0,1,0},Vector3{0,0,1}})
            near(expected.transformVector(axis),tp[pair.target].transformVector(axis));
    }
}
void rotations()
{
    auto a=source(),b=target();Pose reference(a),pose(a),targetReference(b);auto local=pose.locals()[2];local.rotation=multiply(q({0,0,1},.5),local.rotation);pose.setLocal(2,local);
    local=pose.locals()[1];local.rotation=multiply(local.rotation,q({1,0,0},.8));pose.setLocal(1,local);
    const auto before=std::vector<Transform>(pose.locals().begin(),pose.locals().end());const auto rev=pose.revision(),targetRev=targetReference.revision();
    const auto output=retargetPose(reference,pose,targetReference,pairs);CHECK(output.size()==b->bones().size());Pose result(b);
    for(std::size_t i=0;i<output.size();++i){CHECK(output[i].position==targetReference.locals()[i].position&&output[i].scale==targetReference.locals()[i].scale);result.setLocal(i,output[i]);}
    compareWorlds(reference,pose,targetReference,result,pairs);CHECK(pose.revision()==rev&&targetReference.revision()==targetRev);
    CHECK(std::equal(before.begin(),before.end(),pose.locals().begin()));CHECK(output[3]==targetReference.locals()[3]);
    const std::array sharedSource{PoseRetargetJoint{2,2},PoseRetargetJoint{2,1}};
    const auto shared=retargetPose(reference,pose,targetReference,sharedSource);Pose sharedResult(b);
    for(std::size_t i=0;i<shared.size();++i)sharedResult.setLocal(i,shared[i]);
    compareWorlds(reference,pose,targetReference,sharedResult,sharedSource);
}
void basisAndTranslation()
{
    auto a=source(),b=target();Pose reference(a),pose(a),targetReference(b);auto root=pose.locals()[0];root.position={4,5,6};root.rotation=q({0,0,1},.3);pose.setLocal(0,root);
    PoseRetargetOptions options;options.sourceToTargetBasis=q({1,0,0},std::acos(-1.)/2);options.transferRootTranslation=true;options.translationScale=.01;
    const auto output=retargetPose(reference,pose,targetReference,pairs,options);Pose result(b);for(std::size_t i=0;i<output.size();++i)result.setLocal(i,output[i]);
    compareWorlds(reference,pose,targetReference,result,pairs,options.sourceToTargetBasis);
    const auto delta=rotation(options.sourceToTargetBasis).transformVector({3,3,3});near(output[0].position,{5+.01*delta.x,6+.01*delta.y,7+.01*delta.z});
}
void animation()
{
    auto a=source(),b=target();Pose reference(a),animated(a),targetReference(b);auto first=reference.locals()[2],last=first;last.rotation=multiply(q({0,0,1},.8),first.rotation);
    AnimationClip clip(a,1,{{2,{{0,first},{1,last}}}});clip.sample(.5,animated);
    const auto output=retargetPose(reference,animated,targetReference,pairs);Pose result(b);for(std::size_t i=0;i<output.size();++i)result.setLocal(i,output[i]);
    compareWorlds(reference,animated,targetReference,result,pairs);
}
void dynamicBind()
{
    auto a=source(),b=target();Pose reference(a),pose(a),targetReference(b),result(b);
    std::vector<Transform> sourceBind,targetBind;
    for(const auto &bone:a->bones())sourceBind.push_back(bone.bindTransform);
    for(const auto &bone:b->bones())targetBind.push_back(bone.bindTransform);
    sourceBind[0].position={1.5,2.2,3.4};sourceBind[0].rotation=q({0,1,0},.45);sourceBind[0].scale={2,2,2};
    sourceBind[2].position={0,1.5,0};sourceBind[2].rotation=multiply(sourceBind[2].rotation,q({1,0,0},.25));
    targetBind[0].position={4,6.5,7.3};targetBind[0].rotation=multiply(targetBind[0].rotation,q({0,0,1},-.3));targetBind[0].scale={.5,.5,.5};
    targetBind[2].position={0,2.3,0};targetBind[2].rotation=multiply(targetBind[2].rotation,q({0,1,0},.35));
    targetBind[3].rotation=multiply(targetBind[3].rotation,q({1,0,0},-.2));
    reference.setDeformationBind(sourceBind);pose.setDeformationBind(sourceBind);
    targetReference.setDeformationBind(targetBind);result.setDeformationBind(targetBind);
    auto root=pose.locals()[0];root.position={3,1,4};root.rotation=multiply(root.rotation,q({1,0,0},.6));pose.setLocal(0,root);
    auto arm=pose.locals()[2];arm.rotation=multiply(q({0,0,1},.55),arm.rotation);pose.setLocal(2,arm);
    const auto revision=pose.revision(),targetRevision=targetReference.revision();
    PoseRetargetOptions options;options.transferRootTranslation=true;options.translationScale=.5;
    options.sourceToTargetBasis=q({0,0,1},.7);
    const auto verify=[&]
    {
        const auto sourceStart=reference.worldMatrices()[0].transformPoint({});
        const auto sourceEnd=pose.worldMatrices()[0].transformPoint({});
        const auto targetStart=targetReference.worldMatrices()[0].transformPoint({});
        const auto delta=rotation(options.sourceToTargetBasis).transformVector(
            {sourceEnd.x-sourceStart.x,sourceEnd.y-sourceStart.y,sourceEnd.z-sourceStart.z});
        const auto output=retargetPose(reference,pose,targetReference,pairs,options);
        CHECK(output[3]==targetReference.locals()[3]);
        for(std::size_t i=0;i<output.size();++i)result.setLocal(i,output[i]);
        compareWorlds(reference,pose,targetReference,result,pairs,options.sourceToTargetBasis);
        near(result.worldMatrices()[0].transformPoint({}),
             {targetStart.x+.5*delta.x,targetStart.y+.5*delta.y,targetStart.z+.5*delta.z});
    };
    verify();CHECK(pose.revision()==revision&&targetReference.revision()==targetRevision);
    // Differing source reference/current binds contribute their actual world-space change.
    sourceBind[0].position.x+=.4;sourceBind[2].rotation=multiply(sourceBind[2].rotation,q({0,0,1},.2));
    pose.setDeformationBind(sourceBind);verify();
    const auto unchanged=pose.revision();sourceBind[2].scale={2,1,1};pose.setDeformationBind(sourceBind);
    const auto badRevision=pose.revision();CHECK(badRevision>unchanged);
    rejects([&]{(void)retargetPose(reference,pose,targetReference,pairs,options);});CHECK(pose.revision()==badRevision);
    const auto checkBindScale=[&](Vector3 initial,double factor)
    {
        auto bones=std::vector<Bone>(a->bones().begin(),a->bones().end());bones[2].bindTransform.scale=initial;
        auto scaled=std::make_shared<Skeleton>(std::move(bones));Pose base(scaled),sample(scaled),destination(b),sampled(b);
        auto local=base.locals()[2];local.scale={1,1,1};base.setLocal(2,local);sample.setLocal(2,local);
        std::vector<Transform> bind;for(const auto &bone:scaled->bones())bind.push_back(bone.bindTransform);
        bind[2].scale={initial.x*factor,initial.y*factor,initial.z*factor};
        bind[2].rotation=multiply(bind[2].rotation,q({0,0,1},.2));
        base.setDeformationBind(bind);sample.setDeformationBind(bind);
        local=sample.locals()[2];local.rotation=multiply(local.rotation,q({1,0,0},.4));sample.setLocal(2,local);
        const auto output=retargetPose(base,sample,destination,pairs);
        for(std::size_t i=0;i<output.size();++i)sampled.setLocal(i,output[i]);
        compareWorlds(base,sample,destination,sampled,pairs);
    };
    checkBindScale({2,3,4},2);checkBindScale({-1,1,1},1);
}
void validation()
{
    auto a=source(),b=target();Pose reference(a),pose(a),targetReference(b);const auto rev=pose.revision(),targetRev=targetReference.revision();
    auto bad=pairs;bad[1].target=bad[0].target;rejects([&]{(void)retargetPose(reference,pose,targetReference,bad);});
    bad=pairs;bad[0].source=99;rejects([&]{(void)retargetPose(reference,pose,targetReference,bad);});
    bad=pairs;bad[0].target=99;rejects([&]{(void)retargetPose(reference,pose,targetReference,bad);});
    auto other=source();Pose foreign(other);rejects([&]{(void)retargetPose(reference,foreign,targetReference,pairs);});
    PoseRetargetOptions options;options.transferRootTranslation=true;rejects([&]{(void)retargetPose(reference,pose,targetReference,std::span(pairs).first(2),options);});
    options={};options.translationScale=0;rejects([&]{(void)retargetPose(reference,pose,targetReference,pairs,options);});
    options={};options.sourceToTargetBasis={0,0,0,0};rejects([&]{(void)retargetPose(reference,pose,targetReference,pairs,options);});
    options={};options.maximumJoints=2;rejects([&]{(void)retargetPose(reference,pose,targetReference,pairs,options);});
    auto nonuniform=pose.locals()[2];nonuniform.scale={1,2,1};pose.setLocal(2,nonuniform);
    rejects([&]{(void)retargetPose(reference,pose,targetReference,pairs);});CHECK(pose.revision()==rev+1&&targetReference.revision()==targetRev);
    nonuniform.scale={-1,-1,-1};pose.setLocal(2,nonuniform);rejects([&]{(void)retargetPose(reference,pose,targetReference,pairs);});
    pose.reset();Pose badReference(a),badTarget(b);nonuniform=badReference.locals()[1];nonuniform.scale={1,1,3};badReference.setLocal(1,nonuniform);
    rejects([&]{(void)retargetPose(badReference,pose,targetReference,pairs);});
    nonuniform=badTarget.locals()[2];nonuniform.scale={2,1,1};badTarget.setLocal(2,nonuniform);
    rejects([&]{(void)retargetPose(reference,pose,badTarget,pairs);});
    std::vector<Transform> bind;for(const auto &bone:a->bones())bind.push_back(bone.bindTransform);
    bind[0].position.x+=.1;bind[1].scale={2,1,1};pose.setDeformationBind(bind);const auto before=pose.revision();
    rejects([&]{(void)retargetPose(reference,pose,targetReference,pairs);});CHECK(pose.revision()==before);
}
int main(int argc,char **argv)
{
    try{if(argc!=2)return 2;const std::string_view name=argv[1];if(name=="rotations")rotations();else if(name=="basis_translation")basisAndTranslation();else if(name=="animation")animation();else if(name=="dynamic_bind")dynamicBind();else if(name=="validation")validation();else return 2;std::cout<<name<<" passed\n";return 0;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
