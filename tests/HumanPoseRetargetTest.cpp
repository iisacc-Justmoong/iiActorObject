#include <iiActorObject/NSCharacter/HumanPoseRetarget.h>
#include <iiActorObject/NSCharacter/HumanoidGenerator.h>
#include <iiActorObject/AnimationRetarget.h>
#include <iiActorObject/Object3DInterchange.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
auto human(Object3DEngine &engine,double height)
{BodyParameters body;body.height=height;return HumanoidGenerator::generate(engine.reference(),body,{36,false,false});}
HumanJointMapping mapping(const CharacterAsset &asset){return mapHumanJoints(asset,humanJointRules(HumanSkeletonConvention::Procedural));}
void automatic()
{
    Object3DEngine first,second;auto a=human(first,1.7),b=human(second,1.9);const auto source=mapping(*a),target=mapping(*b);
    const auto sourceCount=first.objectCount(),targetCount=second.objectCount();const auto plan=planHumanPoseRetarget(source,target);
    CHECK(plan.skipped.empty()&&plan.sourceMissingRequired.empty()&&plan.targetMissingRequired.empty());
    CHECK(plan.joints.size()==source.bindings().size()&&first.objectCount()==sourceCount&&second.objectCount()==targetCount);
    Pose sourceReference(a->object().get().skeleton()),sourcePose(a->object().get().skeleton()),targetReference(b->object().get().skeleton());
    const auto sourceArm=source.joint("LeftArm"),targetArm=target.joint("LeftArm");auto local=sourcePose.locals()[sourceArm];
    local.rotation=Quaternion::fromAxisAngle({0,0,1},.4);sourcePose.setLocal(sourceArm,local);
    const auto result=retargetPose(sourceReference,sourcePose,targetReference,plan.joints);CHECK(result.size()==targetReference.locals().size());
    CHECK(result[targetArm].position==targetReference.locals()[targetArm].position&&result[targetArm].scale==targetReference.locals()[targetArm].scale);
    CHECK(std::abs(result[targetArm].rotation.z-targetReference.locals()[targetArm].rotation.z)>.1);
    HumanPoseRetargetOptions mappingOptions;mappingOptions.includeRootPair=true;const auto withRoot=planHumanPoseRetarget(source,target,mappingOptions);
    CHECK(withRoot.joints.size()==plan.joints.size()+1&&withRoot.joints.back().source==0&&withRoot.joints.back().target==0);
    auto root=sourcePose.locals()[0];root.position.x+=.25;sourcePose.setLocal(0,root);PoseRetargetOptions poseOptions;poseOptions.transferRootTranslation=true;
    const auto moved=retargetPose(sourceReference,sourcePose,targetReference,withRoot.joints,poseOptions);
    CHECK(std::abs(moved[0].position.x-targetReference.locals()[0].position.x-.25)<1e-10);
}
void partial()
{
    Object3DEngine engine;auto a=human(engine,1.7),b=human(engine,1.9);std::vector<HumanJointRule> sourceRules,targetRules;
    for(const auto &rule:humanJointRules(HumanSkeletonConvention::Procedural))
    {
        if(rule.role=="LeftArm"||rule.role=="LeftForeArm"||rule.role=="LeftHand")sourceRules.push_back(rule);
        if(rule.role=="LeftArm"||rule.role=="LeftForeArm"||rule.role=="RightArm")targetRules.push_back(rule);
    }
    const auto source=mapHumanJoints(*a,sourceRules),target=mapHumanJoints(*b,targetRules);const auto plan=planHumanPoseRetarget(source,target);
    CHECK(plan.joints.size()==2&&plan.skipped.size()==2&&!plan.sourceMissingRequired.empty()&&!plan.targetMissingRequired.empty());
    HumanPoseRetargetOptions options;options.skipMissingRoles=false;rejects([&]{(void)planHumanPoseRetarget(source,target,options);});
    options={};options.maximumRoles=1;rejects([&]{(void)planHumanPoseRetarget(source,target,options);});
    const std::array conflictingRules{HumanJointRule{"Hips","root",HumanJointSource::BoneName}};
    const auto conflicting=mapHumanJoints(*b,conflictingRules);options={};options.includeRootPair=true;
    const auto fullSource=mapping(*a);rejects([&]{(void)planHumanPoseRetarget(fullSource,conflicting,options);});
}
void lifetime()
{
    std::optional<HumanJointMapping> source,target;std::optional<HumanPoseRetargetPlan> plan;
    {Object3DEngine engine;auto a=human(engine,1.7),b=human(engine,1.9);source=mapping(*a);target=mapping(*b);plan=planHumanPoseRetarget(*source,*target);}
    CHECK(!source->valid()&&!target->valid()&&!plan->joints.empty());rejects([&]{(void)planHumanPoseRetarget(*source,*target);});
}
void assets(const std::filesystem::path &sourcePath,const std::filesystem::path &targetPath)
{
    const auto read=[](const std::filesystem::path &path)
    {
        std::ifstream stream(path,std::ios::binary|std::ios::ate);const auto size=stream.tellg();CHECK(size>0&&size<256*1024*1024);
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));stream.seekg(0);CHECK(bool(stream.read(reinterpret_cast<char*>(bytes.data()),size)));return bytes;
    };
    Object3DEngine first,second;CharacterAsset a(importObject3DFbx(first.reference(),read(sourcePath)).object);
    CharacterAsset b(importObject3DFbx(second.reference(),read(targetPath)).object);
    const auto sourceMap=mapHumanJoints(a,humanJointRules(HumanSkeletonConvention::MakeHuman));
    const auto targetMap=mapHumanJoints(b,humanJointRules(HumanSkeletonConvention::MakeHuman));const auto plan=planHumanPoseRetarget(sourceMap,targetMap);
    CHECK(plan.skipped.empty()&&plan.sourceMissingRequired.empty()&&plan.targetMissingRequired.empty()&&!plan.joints.empty());
    HumanPoseRetargetOptions rootOptions;rootOptions.includeRootPair=true;const auto withRoot=planHumanPoseRetarget(sourceMap,targetMap,rootOptions);
    CHECK(withRoot.joints.size()==plan.joints.size());
    CHECK(std::count_if(withRoot.joints.begin(),withRoot.joints.end(),[](const auto &joint){return joint.target==0&&joint.source==0;})==1);
    Pose sourceReference(a.object().get().skeleton()),sourcePose(a.object().get().skeleton()),targetReference(b.object().get().skeleton());
    const auto arm=sourceMap.joint("LeftArm");auto local=sourcePose.locals()[arm];local.rotation=Quaternion::fromAxisAngle({0,0,1},.37);sourcePose.setLocal(arm,local);
    const auto result=retargetPose(sourceReference,sourcePose,targetReference,plan.joints);
    CHECK(result.size()==163);for(std::size_t i=0;i<result.size();++i)
    {CHECK(result[i].position==targetReference.locals()[i].position&&result[i].scale==targetReference.locals()[i].scale);}
    const AnimationClip motion(sourceReference.skeleton(),1.,{{arm,{{0.,sourceReference.locals()[arm]},{1.,local}}}});
    const auto baked=retargetAnimation(motion,sourceReference,targetReference,plan.joints,std::array{0.,.5,1.});
    Pose sampled(targetReference.skeleton());baked.sample(1.,sampled);
    CHECK(baked.tracks().size()==163);
    for(std::size_t i=0;i<result.size();++i)
    {
        const auto actual=sampled.locals()[i].matrix(),expected=result[i].matrix();
        for(const auto axis:std::array{Vector3{1,0,0},Vector3{0,1,0},Vector3{0,0,1}})
        {
            const auto a=actual.transformVector(axis),b=expected.transformVector(axis);
            CHECK(std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)<1e-9);
        }
        CHECK(sampled.locals()[i].position==result[i].position);
    }
    Pose direct(targetReference.skeleton());
    for(std::size_t i=0;i<result.size();++i)direct.setLocal(i,result[i]);
    std::size_t moved=0;
    for(const auto &mesh:b.object().get().meshes())
    {
        const auto &lod=mesh->lods()[0];std::vector<double> weights(lod.morphTargets.size(),0.);
        std::vector<DeformedVertex> actual(lod.vertices.size()),expected(lod.vertices.size()),rest(lod.vertices.size());
        mesh->deform(sampled,0,weights,actual);mesh->deform(direct,0,weights,expected);
        mesh->deform(targetReference,0,weights,rest);
        for(std::size_t i=0;i<actual.size();++i)
        {
            const auto a=actual[i].position,e=expected[i].position,r=rest[i].position;
            CHECK(std::hypot(a.x-e.x,a.y-e.y,a.z-e.z)<1e-8);
            if(std::hypot(a.x-r.x,a.y-r.y,a.z-r.z)>1e-6)++moved;
        }
    }
    CHECK(moved>0);
    const auto glb=exportObject3DGlb(b.object().get(),baked);
    const std::string marker="\"animations\":[";
    CHECK(glb.size()>20&&glb[0]=='g'&&glb[1]=='l'&&glb[2]=='T'&&glb[3]=='F');
    CHECK(std::search(glb.begin(),glb.end(),marker.begin(),marker.end())!=glb.end());
    // Shape-dependent binds on both characters retain the role mapping and
    // compare each mapped world rotation with independent matrix composition.
    Pose shapedSourceReference(a.skeleton()),shapedSource(a.skeleton()),shapedTargetReference(b.skeleton()),shapedResult(b.skeleton());
    std::vector<Transform> sourceBind,targetBind;
    for(const auto &bone:a.skeleton()->bones())sourceBind.push_back(bone.bindTransform);
    for(const auto &bone:b.skeleton()->bones())targetBind.push_back(bone.bindTransform);
    const auto targetArm=targetMap.joint("LeftArm");CHECK(arm!=NoBone&&targetArm!=NoBone);
    sourceBind[arm].position.x+=.02;targetBind[targetArm].position.y+=.03;
    shapedSourceReference.setDeformationBind(sourceBind);shapedSource.setDeformationBind(sourceBind);
    shapedTargetReference.setDeformationBind(targetBind);shapedResult.setDeformationBind(targetBind);
    auto shapedArm=shapedSource.locals()[arm];shapedArm.rotation=Quaternion::fromAxisAngle({0,0,1},.37);
    shapedSource.setLocal(arm,shapedArm);
    const auto shapedLocals=retargetPose(shapedSourceReference,shapedSource,shapedTargetReference,plan.joints);
    for(std::size_t i=0;i<shapedLocals.size();++i)shapedResult.setLocal(i,shapedLocals[i]);
    const auto sw=shapedSource.worldMatrices(),sr=shapedSourceReference.worldMatrices();
    const auto tr=shapedTargetReference.worldMatrices(),tw=shapedResult.worldMatrices();double worst=0;
    for(const auto &pair:plan.joints)
    {
        const auto expectedRotation=sw[pair.source]*sr[pair.source].invertedAffine()*tr[pair.target];
        for(const auto axis:std::array{Vector3{1,0,0},Vector3{0,1,0},Vector3{0,0,1}})
        {
            const auto x=expectedRotation.transformVector(axis),y=tw[pair.target].transformVector(axis);
            worst=std::max(worst,std::hypot(x.x-y.x,x.y-y.y,x.z-y.z));
        }
    }
    CHECK(worst<1e-8);
    const AnimationClip shapedMotion(a.skeleton(),1.,{{arm,{{0.,shapedSourceReference.locals()[arm]},{1.,shapedArm}}}});
    const auto shapedClip=retargetAnimation(shapedMotion,shapedSourceReference,shapedTargetReference,plan.joints,std::array{0.,.5,1.});
    CHECK(!shapedClip.requiredDeformationBind().empty());Pose shapedSample(b.skeleton());
    rejects([&]{shapedClip.sample(1.,shapedSample);});shapedSample.setDeformationBind(targetBind);shapedClip.sample(1.,shapedSample);
    for(std::size_t i=0;i<shapedLocals.size();++i)
    {
        const auto a=shapedSample.worldMatrices()[i].transformPoint({}),b=shapedResult.worldMatrices()[i].transformPoint({});
        CHECK(std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)<1e-8);
    }
    rejects([&]{(void)exportObject3DGlb(b.object().get(),shapedClip);});
    std::cout<<"dynamic_roles="<<plan.joints.size()<<" dynamic_world_rotation_error="<<worst<<'\n';
}
int main(int argc,char **argv)
{
    try{if(argc<2)return 2;const std::string_view name=argv[1];if(name=="automatic")automatic();else if(name=="partial")partial();else if(name=="lifetime")lifetime();else if(name=="assets"&&argc==4)assets(argv[2],argv[3]);else return 2;std::cout<<name<<" passed\n";return 0;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
