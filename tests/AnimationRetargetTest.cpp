#include <iiActorObject/AnimationRetarget.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace iiActorObject;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)
template<class F> void rejects(F f)
{
    bool caught = false;
    try { f(); } catch (const std::exception &) { caught = true; }
    CHECK(caught);
}
std::shared_ptr<Skeleton> sourceSkeleton()
{
    return std::make_shared<Skeleton>(std::vector<Bone>{
        {"root", NoBone, {{1,2,3}}}, {"tip",2,{{1,0,0}}}, {"arm",0,{{0,1,0}}}});
}
std::shared_ptr<Skeleton> targetSkeleton()
{
    return std::make_shared<Skeleton>(std::vector<Bone>{
        {"root", NoBone, {{5,6,7}}}, {"tip",3,{{2,0,0}}},
        {"arm",0,{{0,2,0}}}, {"helper",2,{{.3,0,0}}}});
}
constexpr std::array pairs{PoseRetargetJoint{0,0}, PoseRetargetJoint{2,2}, PoseRetargetJoint{1,1}};
AnimationClip sourceClip(const std::shared_ptr<Skeleton> &skeleton)
{
    auto root = skeleton->bones()[0].bindTransform;
    auto moved = root; moved.position = {5,2,3};
    auto arm = skeleton->bones()[2].bindTransform;
    auto turned = arm; turned.rotation = Quaternion::fromAxisAngle({0,0,1},1.0);
    return AnimationClip(skeleton,2.0,{{0,{{0,root},{2,moved}}},{2,{{0,arm},{2,turned}}}},
                         {{"expression",{{0,.2},{1,.8},{2,.4}}}});
}
void nearVector(Vector3 a,Vector3 b)
{
    CHECK(std::hypot(a.x-b.x,a.y-b.y,a.z-b.z) < 1e-9);
}
void nearTransform(const Transform &a,const Transform &b)
{
    nearVector(a.position,b.position); nearVector(a.scale,b.scale);
    const auto am=a.matrix(),bm=b.matrix();
    for (const auto axis : std::array{Vector3{1,0,0},Vector3{0,1,0},Vector3{0,0,1}})
        nearVector(am.transformVector(axis),bm.transformVector(axis));
}
void samples()
{
    auto source=sourceSkeleton(),target=targetSkeleton();
    auto clip=sourceClip(source); Pose sourceReference(source),targetReference(target);
    // An untracked source bone samples the skeleton's bind, not this reference.
    auto sourceTip=sourceReference.locals()[1];sourceTip.rotation=Quaternion::fromAxisAngle({0,1,0},.2);
    sourceReference.setLocal(1,sourceTip);
    auto helper=targetReference.locals()[3]; helper.rotation=Quaternion::fromAxisAngle({1,0,0},.3);
    targetReference.setLocal(3,helper);
    const auto sourceRevision=sourceReference.revision(),targetRevision=targetReference.revision();
    constexpr std::array times{0.,.4,1.2,2.};
    AnimationRetargetOptions options; options.pose.transferRootTranslation=true; options.pose.translationScale=.5;
    options.pose.sourceToTargetBasis=Quaternion::fromAxisAngle({0,0,1},std::acos(-1.)/2);
    const auto baked=retargetAnimation(clip,sourceReference,targetReference,pairs,times,options);
    CHECK(baked.skeleton().get()==target.get() && baked.duration()==2.0);
    CHECK(baked.tracks().size()==target->bones().size() && baked.curves().empty());
    for (std::size_t i=0;i<baked.tracks().size();++i)
    {
        CHECK(baked.tracks()[i].bone==i && baked.tracks()[i].keys.size()==times.size());
        for (std::size_t k=0;k<times.size();++k) CHECK(baked.tracks()[i].keys[k].seconds==times[k]);
    }
    Pose animated(source),result(target);
    for (double time:times)
    {
        clip.sample(time,animated);
        const auto expected=retargetPose(sourceReference,animated,targetReference,pairs,options.pose);
        baked.sample(time,result);
        for (std::size_t i=0;i<expected.size();++i) nearTransform(result.locals()[i],expected[i]);
        nearTransform(result.locals()[3],helper);
    }
    nearVector(result.locals()[0].position,{5,8,7});
    CHECK(sourceReference.revision()==sourceRevision && targetReference.revision()==targetRevision);
}
void curves()
{
    auto source=sourceSkeleton(),target=targetSkeleton();auto clip=sourceClip(source);
    Pose sourceReference(source),targetReference(target);constexpr std::array times{0.,1.,2.};
    const auto without=retargetAnimation(clip,sourceReference,targetReference,pairs,times);
    CHECK(without.curves().empty());
    AnimationRetargetOptions options;options.copyScalarCurves=true;
    const auto with=retargetAnimation(clip,sourceReference,targetReference,pairs,times,options);
    CHECK(with.curves().size()==1 && with.curves()[0].name=="expression");
    CHECK(with.curves()[0].keys.size()==3);
    for(double t:std::array{0.,.25,1.,1.5,2.})
        CHECK(std::abs(with.sampleCurve(0,t)-clip.sampleCurve(0,t))<1e-12);
    Pose result(target);with.sample(.5,result);
    CHECK(std::abs(result.locals()[2].rotation.z-std::sin(.125))<1e-10);
    const auto unmapped=retargetAnimation(clip,sourceReference,targetReference,std::span<const PoseRetargetJoint>{},times);
    unmapped.sample(1.,result);
    for(std::size_t i=0;i<result.locals().size();++i)nearTransform(result.locals()[i],targetReference.locals()[i]);
}
void dynamicBind()
{
    auto source=sourceSkeleton(),target=targetSkeleton();auto clip=sourceClip(source);
    Pose sourceReference(source),targetReference(target),animated(source),actual(target),expected(target);
    std::vector<Transform> sourceBind,targetBind;
    for(const auto &bone:source->bones())sourceBind.push_back(bone.bindTransform);
    for(const auto &bone:target->bones())targetBind.push_back(bone.bindTransform);
    sourceBind[0].position.x+=.5;sourceBind[0].rotation=Quaternion::fromAxisAngle({0,1,0},.3);
    sourceBind[2].position.y+=.4;targetBind[0].position.z+=.7;
    targetBind[0].rotation=Quaternion::fromAxisAngle({1,0,0},.4);
    targetBind[2].rotation=Quaternion::fromAxisAngle({0,0,1},-.2);
    sourceReference.setDeformationBind(sourceBind);animated.setDeformationBind(sourceBind);
    targetReference.setDeformationBind(targetBind);expected.setDeformationBind(targetBind);
    constexpr std::array times{0.,.5,1.,2.};AnimationRetargetOptions options;
    options.pose.transferRootTranslation=true;options.pose.translationScale=.6;
    options.pose.sourceToTargetBasis=Quaternion::fromAxisAngle({0,0,1},.6);
    const auto baked=retargetAnimation(clip,sourceReference,targetReference,pairs,times,options);
    CHECK(!baked.requiredDeformationBind().empty());
    const auto revision=actual.revision();rejects([&]{baked.sample(1.,actual);});CHECK(actual.revision()==revision);
    actual.setDeformationBind(targetBind);
    for(const double time:times)
    {
        clip.sample(time,animated);const auto locals=retargetPose(sourceReference,animated,targetReference,pairs,options.pose);
        for(std::size_t i=0;i<locals.size();++i)expected.setLocal(i,locals[i]);
        baked.sample(time,actual);
        for(std::size_t i=0;i<locals.size();++i)
        {
            nearTransform(actual.locals()[i],locals[i]);
            for(const auto point:std::array{Vector3{},Vector3{.2,.3,.4}})
                nearVector(actual.worldMatrices()[i].transformPoint(point),expected.worldMatrices()[i].transformPoint(point));
        }
    }
    auto equivalent=targetBind;auto &rotation=equivalent[0].rotation;
    rotation={-rotation.x,-rotation.y,-rotation.z,-rotation.w};actual.setDeformationBind(equivalent);baked.sample(1.,actual);
    auto wrong=targetBind;wrong[0].position.x+=.1;actual.setDeformationBind(wrong);const auto before=actual.revision();
    rejects([&]{baked.sample(1.,actual);});CHECK(actual.revision()==before);
    rejects([&]{AnimationClip invalid(target,2.,{}, {},std::vector<Transform>{{}});});
    auto invalidBind=targetBind;invalidBind[1].scale.x=0;
    rejects([&]{AnimationClip invalid(target,2.,{}, {},invalidBind);});
    auto unnormalized=targetBind;auto &unnormalizedRotation=unnormalized[0].rotation;
    unnormalizedRotation={2*unnormalizedRotation.x,2*unnormalizedRotation.y,2*unnormalizedRotation.z,2*unnormalizedRotation.w};
    const AnimationClip normalized(target,2.,{}, {},unnormalized);
    const auto q=normalized.requiredDeformationBind()[0].rotation;
    CHECK(std::abs(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w-1)<1e-12);
    auto playerPose=std::make_shared<Pose>(target);playerPose->setDeformationBind(targetBind);
    AnimationPlayer player(playerPose);player.play(std::make_shared<AnimationClip>(baked));
    playerPose->setDeformationBind(wrong);const auto oldTime=player.time();const auto oldRevision=playerPose->revision();
    rejects([&]{player.seek(1);});CHECK(player.time()==oldTime&&playerPose->revision()==oldRevision);
    const AnimationClip requiredSource(source,clip.duration(),std::vector<BoneTrack>(clip.tracks().begin(),clip.tracks().end()),
        std::vector<AnimationCurve>(clip.curves().begin(),clip.curves().end()),sourceBind);
    Pose missingSourceBind(source);const auto missingRevision=missingSourceBind.revision();
    rejects([&]{(void)retargetAnimation(requiredSource,missingSourceBind,targetReference,pairs,times,options);});
    CHECK(missingSourceBind.revision()==missingRevision);
}
void validation()
{
    auto source=sourceSkeleton(),target=targetSkeleton();auto clip=sourceClip(source);
    Pose sourceReference(source),targetReference(target),foreign(sourceSkeleton());
    constexpr std::array good{0.,1.,2.};
    const auto fail=[&](auto times){rejects([&]{(void)retargetAnimation(clip,sourceReference,targetReference,pairs,times);});};
    fail(std::array<double,0>{});fail(std::array{0.});fail(std::array{.1,2.});fail(std::array{0.,1.});
    fail(std::array{0.,1.,1.,2.});fail(std::array{0.,1.5,1.,2.});
    fail(std::array{0.,std::numeric_limits<double>::quiet_NaN(),2.});
    rejects([&]{(void)retargetAnimation(clip,foreign,targetReference,pairs,good);});
    auto duplicate=pairs;duplicate[1].target=duplicate[0].target;
    rejects([&]{(void)retargetAnimation(clip,sourceReference,targetReference,duplicate,good);});
    AnimationRetargetOptions options;options.maximumSamples=2;
    rejects([&]{(void)retargetAnimation(clip,sourceReference,targetReference,pairs,good,options);});
    options.maximumSamples=1;
    rejects([&]{(void)retargetAnimation(clip,sourceReference,targetReference,pairs,good,options);});
    options.maximumSamples=1'000'001;
    rejects([&]{(void)retargetAnimation(clip,sourceReference,targetReference,pairs,good,options);});
    options={};options.maximumOutputKeys=11;
    rejects([&]{(void)retargetAnimation(clip,sourceReference,targetReference,pairs,good,options);});
    options.maximumOutputKeys=12;options.copyScalarCurves=true;
    rejects([&]{(void)retargetAnimation(clip,sourceReference,targetReference,pairs,good,options);});
    options={};options.maximumOutputKeys=10'000'001;
    rejects([&]{(void)retargetAnimation(clip,sourceReference,targetReference,pairs,good,options);});
    options={};options.pose.maximumJoints=2;
    rejects([&]{(void)retargetAnimation(clip,sourceReference,targetReference,pairs,good,options);});
    auto first=source->bones()[2].bindTransform,last=first;last.scale={1,2,1};
    AnimationClip scaled(source,2.,{{2,{{0,first},{2,last}}}});
    const auto sourceRevision=sourceReference.revision(),targetRevision=targetReference.revision();
    rejects([&]{(void)retargetAnimation(scaled,sourceReference,targetReference,pairs,good);});
    CHECK(sourceReference.revision()==sourceRevision && targetReference.revision()==targetRevision);
}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=2)return 2;
        const std::string_view name=argv[1];
        if(name=="samples")samples(); else if(name=="curves")curves();else if(name=="dynamic_bind")dynamicBind();
        else if(name=="validation")validation(); else return 2;
        std::cout<<name<<" passed\n"; return 0;
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
