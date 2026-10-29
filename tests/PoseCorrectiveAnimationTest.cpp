#include <iiActorObject/Object3DPoseCorrectiveAnimation.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#endif
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string_view>

using namespace iiActorObject;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x); } while (false)
template<class F> void rejects(F f)
{
    bool caught=false;try{f();}catch(const std::exception &){caught=true;}CHECK(caught);
}
bool near(double a,double b,double tolerance=1e-9){return std::abs(a-b)<=tolerance;}
Quaternion product(Quaternion a,Quaternion b)
{
    return Quaternion{a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
        a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
        a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z}.normalized();
}
class SpoofedDriver final:public Object3DRigDriver
{
  public:
    SpoofedDriver(){definition_.sourceFormat="iiActorObject.poseCorrective.1";definition_.joints={"elbow"};definition_.blendShapes={"fold"};}
    const Object3DRigDefinition &definition()const noexcept override{return definition_;}
    std::unique_ptr<Object3DRigDriver> clone()const override{return std::make_unique<SpoofedDriver>(*this);}
    Object3DRigOutputs evaluate(std::span<const double>,RigControlSpace,std::size_t)override{return {};}
  private:Object3DRigDefinition definition_;
};
Object3DData fixtureData()
{
    auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    data.kind=Object3DKind::SkeletalMesh;
    data.bones={{"root",NoBone,{}},{"elbow",0,{{0,1,0}}}};
    auto &body=data.meshes[0];body.name="body";
    for(auto &point:body.topology->points)point.weights={{1,1}};
    body.topology->morphTargets={{"fold",{{0,{0,0,.2}}}},{"keep",{{1,{0,.1,0}}}}};
    auto garment=body;garment.name="garment";data.meshes.push_back(std::move(garment));
    return data;
}
Object3DPoseCorrectiveProgram program(double center=std::numbers::pi/2,double radius=std::numbers::pi/2)
{
    return {{{"fold",{{"elbow",Quaternion::fromAxisAngle({1,0,0},center),radius}},1}}};
}
AnimationClip motion(const std::shared_ptr<const Skeleton> &skeleton,bool collision=false,
                     std::vector<Transform> bind={})
{
    auto start=skeleton->bones()[1].bindTransform,end=start;
    end.rotation=Quaternion::fromAxisAngle({1,0,0},std::numbers::pi/2);
    std::vector<AnimationCurve> curves{{"keep",{{0,.2},{1,.8}}}};
    if(collision)curves.push_back({"fold",{{0,.7},{1,.7}}});
    return {skeleton,1,{{1,{{0,start},{1,end}}}},std::move(curves),std::move(bind)};
}
std::size_t curveIndex(const AnimationClip &clip,std::string_view name)
{
    for(std::size_t i=0;i<clip.curves().size();++i)if(clip.curves()[i].name==name)return i;
    throw std::runtime_error("Missing curve");
}
void accuracy()
{
    Object3DEngine engine;const auto object=engine.createObject(fixtureData());
    const auto rig=createObject3DPoseCorrectiveRig(object,program());const auto input=motion(object.get().skeleton());
    PoseCorrectiveAnimationOptions options;options.maximumSampleInterval=.3;options.maximumWeightError=.0002;
    const auto result=bakeObject3DPoseCorrectiveAnimation(rig,input,options);
    CHECK(result.clip.skeleton()==input.skeleton()&&result.clip.duration()==input.duration());
    CHECK(result.clip.tracks().size()==1&&result.clip.tracks()[0].keys.size()==2);
    CHECK(result.clip.tracks()[0].keys[1].value==input.tracks()[0].keys[1].value);
    CHECK(input.curves().size()==1&&input.curves()[0].name=="keep");
    CHECK(result.clip.curves().size()==2&&result.clip.curves()[0].name=="keep");
    CHECK(result.sampleTimes>5&&result.evaluations>=result.sampleTimes);
    CHECK(result.maximumObservedWeightError<=options.maximumWeightError);
    CHECK(result.maximumConservativeWeightError&&*result.maximumConservativeWeightError<=options.maximumWeightError);
    const auto fold=curveIndex(result.clip,"fold"),keep=curveIndex(result.clip,"keep");
    Pose pose(input.skeleton());double maximumError=0;
    for(int i=0;i<=1000;++i)
    {
        const double t=i/1000.;input.sample(t,pose);
        const double expected=evaluateObject3DPoseCorrectives(rig,pose).blendShapes[0];
        maximumError=std::max(maximumError,std::abs(result.clip.sampleCurve(fold,t)-expected));
        CHECK(near(result.clip.sampleCurve(keep,t),input.sampleCurve(0,t)));
    }
    CHECK(maximumError<.00025);
    CHECK(near(result.clip.sampleCurve(fold,0),0)&&near(result.clip.sampleCurve(fold,.5),.5,1e-8));
    CHECK(near(result.clip.sampleCurve(fold,1),1));
}
void validation()
{
    Object3DEngine engine;const auto object=engine.createObject(fixtureData());
    const auto rig=createObject3DPoseCorrectiveRig(object,program());const auto input=motion(object.get().skeleton());
    const auto collision=motion(object.get().skeleton(),true);
    rejects([&]{(void)bakeObject3DPoseCorrectiveAnimation(rig,collision);});
    PoseCorrectiveAnimationOptions options;options.collision=PoseCorrectiveCurveCollision::Replace;
    const auto replaced=bakeObject3DPoseCorrectiveAnimation(rig,collision,options);
    CHECK(replaced.clip.curves().size()==2&&collision.curves().size()==2);
    CHECK(near(collision.sampleCurve(1,0),.7)&&near(replaced.clip.sampleCurve(curveIndex(replaced.clip,"fold"),0),0));
    CHECK(near(replaced.clip.sampleCurve(curveIndex(replaced.clip,"keep"),.5),.5));
    options={};options.maximumSampleInterval=0;rejects([&]{(void)bakeObject3DPoseCorrectiveAnimation(rig,input,options);});
    options={};options.maximumWeightError=std::numeric_limits<double>::quiet_NaN();rejects([&]{(void)bakeObject3DPoseCorrectiveAnimation(rig,input,options);});
    options={};options.maximumSampleTimes=2;rejects([&]{(void)bakeObject3DPoseCorrectiveAnimation(rig,input,options);});
    options={};options.maximumEvaluations=2;rejects([&]{(void)bakeObject3DPoseCorrectiveAnimation(rig,input,options);});
    options={};options.maximumEvaluatedWeights=2;rejects([&]{(void)bakeObject3DPoseCorrectiveAnimation(rig,input,options);});
    options={};options.maximumStoredJointRotations=1;rejects([&]{(void)bakeObject3DPoseCorrectiveAnimation(rig,input,options);});
    options={};options.maximumBoneVisits=2;rejects([&]{(void)bakeObject3DPoseCorrectiveAnimation(rig,input,options);});
    options={};options.maximumRigOperations=1;rejects([&]{(void)bakeObject3DPoseCorrectiveAnimation(rig,input,options);});
    options={};options.maximumSourceKeyVisits=1;rejects([&]{(void)bakeObject3DPoseCorrectiveAnimation(rig,input,options);});
    options={};options.maximumSubdivisionDepth=0;options.maximumSampleInterval=1;options.maximumWeightError=1e-8;
    rejects([&]{(void)bakeObject3DPoseCorrectiveAnimation(rig,input,options);});
    options={};options.collision=static_cast<PoseCorrectiveCurveCollision>(100);
    rejects([&]{(void)bakeObject3DPoseCorrectiveAnimation(rig,input,options);});
    options={};options.sampling=static_cast<PoseCorrectiveSampling>(100);
    rejects([&]{(void)bakeObject3DPoseCorrectiveAnimation(rig,input,options);});
    Object3DEngine other;const auto foreign=other.createObject(fixtureData());
    rejects([&]{(void)bakeObject3DPoseCorrectiveAnimation(rig,motion(foreign.get().skeleton()));});
    rejects([&]{(void)bakeObject3DPoseCorrectiveAnimation({},input);});
    const auto spoof=engine.reference().createRigSession(object,std::make_unique<SpoofedDriver>());
    rejects([&]{(void)bakeObject3DPoseCorrectiveAnimation(spoof,input);});
    CHECK(engine.rigCount()==2&&object.valid()&&input.curves().size()==1);
    CHECK(engine.eraseRig(rig.id()));rejects([&]{(void)bakeObject3DPoseCorrectiveAnimation(rig,input);});
}
void bound()
{
    Object3DEngine engine;const auto object=engine.createObject(fixtureData());
    const auto rig=createObject3DPoseCorrectiveRig(object,program());
    std::vector<Transform> bind;for(const auto &bone:object.get().skeleton()->bones())bind.push_back(bone.bindTransform);
    bind[1].position.x=.25;const auto input=motion(object.get().skeleton(),false,bind);
    const auto result=bakeObject3DPoseCorrectiveAnimation(rig,input);
    CHECK(result.clip.requiredDeformationBind().size()==bind.size());
    CHECK(result.clip.requiredDeformationBind()[1].position.x==.25);
    Pose pose(object.get().skeleton());rejects([&]{result.clip.sample(.5,pose);});
    (void)pose.setDeformationBind(bind);result.clip.sample(.5,pose);
    CHECK(near(result.clip.sampleCurve(curveIndex(result.clip,"fold"),.5),.5,1e-8));
    const AnimationClip still(object.get().skeleton(),1,{},{});
    const auto constant=bakeObject3DPoseCorrectiveAnimation(rig,still);
    CHECK(near(constant.clip.sampleCurve(curveIndex(constant.clip,"fold"),.5),0));
    const auto inactive=createObject3DPoseCorrectiveRig(object,program(1,.1));
    PoseCorrectiveAnimationOptions exact;exact.maximumSampleInterval=1;exact.maximumWeightError=1e-16;exact.maximumSubdivisionDepth=0;
    const auto zero=bakeObject3DPoseCorrectiveAnimation(inactive,still,exact);
    CHECK(zero.maximumConservativeWeightError&&*zero.maximumConservativeWeightError==0);
    CHECK(zero.clip.sampleCurve(curveIndex(zero.clip,"fold"),.5)==0);
}
void narrow()
{
    Object3DEngine engine;const auto object=engine.createObject(fixtureData());
    const auto rig=createObject3DPoseCorrectiveRig(object,program(.3*std::numbers::pi/2,.02));
    const auto input=motion(object.get().skeleton());PoseCorrectiveAnimationOptions options;
    options.maximumSampleInterval=1;options.maximumWeightError=.001;
    const auto coarse=bakeObject3DPoseCorrectiveAnimation(rig,input,options);
    CHECK(coarse.maximumConservativeWeightError&&*coarse.maximumConservativeWeightError<=options.maximumWeightError);
    CHECK(coarse.clip.sampleCurve(curveIndex(coarse.clip,"fold"),.3)>.999);
    auto limited=options;limited.maximumSubdivisionDepth=0;
    rejects([&]{(void)bakeObject3DPoseCorrectiveAnimation(rig,input,limited);});
    Pose pose(input.skeleton());
    for(int i=0;i<=4000;++i)
    {
        const double time=.28+i*.00001;input.sample(time,pose);
        const auto expected=evaluateObject3DPoseCorrectives(rig,pose).blendShapes[0];
        const auto actual=coarse.clip.sampleCurve(curveIndex(coarse.clip,"fold"),time);
        CHECK(near(actual,expected,*coarse.maximumConservativeWeightError+1e-9));
    }
    options.sampling=PoseCorrectiveSampling::ProbeOnly;
    const auto fast=bakeObject3DPoseCorrectiveAnimation(rig,input,options);
    CHECK(!fast.maximumConservativeWeightError&&fast.maximumObservedWeightError<1e-9);
    CHECK(fast.clip.sampleCurve(curveIndex(fast.clip,"fold"),.3)<.01);
    options.maximumSampleInterval=.025;
    const auto result=bakeObject3DPoseCorrectiveAnimation(rig,input,options);
    CHECK(result.clip.sampleCurve(curveIndex(result.clip,"fold"),.3)>.99);
}
void conservative()
{
    auto data=fixtureData();data.bones.push_back({"wrist",1,{{0,1,0}}});
    Object3DEngine engine;const auto object=engine.createObject(std::move(data));
    Object3DPoseCorrectiveProgram rules{{
        {"fold",{{"elbow",Quaternion::fromAxisAngle({1,0,0},.471238898038),.015},
                 {"wrist",Quaternion::fromAxisAngle({0,1,0},.00666666667),.004}},.8},
        {"fold",{{"elbow",Quaternion::fromAxisAngle({1,0,0},1.0),.1}},.6},
        {"keep",{{"wrist",Quaternion::fromAxisAngle({0,1,0},.021),.01}},.9}}};
    const auto rig=createObject3DPoseCorrectiveRig(object,rules);
    auto elbow=object.get().skeleton()->bones()[1].bindTransform,elbowEnd=elbow;
    elbowEnd.rotation=Quaternion::fromAxisAngle({1,0,0},std::numbers::pi/2);
    auto wrist=object.get().skeleton()->bones()[2].bindTransform,wristEnd=wrist;
    wristEnd.rotation=Quaternion::fromAxisAngle({0,1,0},.04);
    const AnimationClip clip(object.get().skeleton(),1,{{1,{{0,elbow},{1,elbowEnd}}},
        {2,{{.2,wrist},{.8,wristEnd}}}});
    PoseCorrectiveAnimationOptions options;options.maximumSampleInterval=1;options.maximumWeightError=.003;
    const auto baked=bakeObject3DPoseCorrectiveAnimation(rig,clip,options);
    CHECK(baked.maximumConservativeWeightError&&*baked.maximumConservativeWeightError<=options.maximumWeightError);
    CHECK(baked.clip.sampleCurve(curveIndex(baked.clip,"fold"),.3)>.79);
    Pose pose(clip.skeleton());
    for(int i=0;i<=10000;++i)
    {
        const double time=i/10000.;clip.sample(time,pose);
        const auto expected=evaluateObject3DPoseCorrectives(rig,pose);
        for(std::size_t channel=0;channel<expected.blendShapes.size();++channel)
            CHECK(near(baked.clip.sampleCurve(channel,time),expected.blendShapes[channel],
                *baked.maximumConservativeWeightError+1e-9));
    }
    options.maximumRigOperations=8;rejects([&]{(void)bakeObject3DPoseCorrectiveAnimation(rig,clip,options);});
}
void conservativePaths()
{
    auto data=fixtureData();data.bones[1].bindTransform.rotation=Quaternion::fromAxisAngle({0,0,1},.35);
    data.bones.push_back({"wrist",1,{{0,1,0},Quaternion::fromAxisAngle({1,0,0},-.2)}});
    Object3DEngine engine;const auto object=engine.createObject(std::move(data));
    const auto skeleton=object.get().skeleton();
    for(int trial=0;trial<8;++trial)
    {
        auto first=skeleton->bones()[1].bindTransform,mid=first,last=first;
        first.rotation=product(first.rotation,Quaternion::fromAxisAngle({1,2,3},-.15-.02*trial));
        mid.rotation=product(mid.rotation,Quaternion::fromAxisAngle({3,-1,2},.5+.03*trial));
        last.rotation=product(last.rotation,Quaternion::fromAxisAngle({-1,3,1},1.1-.04*trial));
        if(trial==7)last.rotation=product(mid.rotation,Quaternion::fromAxisAngle({1,2,3},std::numbers::pi));
        if(trial%2)last.rotation={-last.rotation.x,-last.rotation.y,-last.rotation.z,-last.rotation.w};
        auto wrist=skeleton->bones()[2].bindTransform,wristEnd=wrist;
        wristEnd.rotation=product(wrist.rotation,Quaternion::fromAxisAngle({0,1,0},.04+.001*trial));
        const AnimationClip clip(skeleton,1,{{1,{{0,first},{.371,mid},{1,last}}},
            {2,{{.13,wrist},{.83,wristEnd}}}});
        Pose pose(skeleton);const double center=.17+.065*trial;clip.sample(center,pose);
        auto relative=[&](BoneIndex bone)
        {
            const auto bind=skeleton->bones()[bone].bindTransform.rotation;
            return product(Quaternion{-bind.x,-bind.y,-bind.z,bind.w},pose.locals()[bone].rotation);
        };
        const auto elbowTarget=relative(1),wristTarget=relative(2);
        Object3DPoseCorrectiveProgram rules{{
            {"fold",{{"elbow",elbowTarget,.08+.01*trial},{"wrist",wristTarget,.004}},.8},
            {"fold",{{"elbow",Quaternion::fromAxisAngle({1,0,0},.8),.3}},.4},
            {"keep",{{"wrist",wristTarget,.008}},.7}}};
        const auto rig=createObject3DPoseCorrectiveRig(object,rules);
        PoseCorrectiveAnimationOptions options;options.maximumSampleInterval=.5;options.maximumWeightError=.006;
        const auto baked=bakeObject3DPoseCorrectiveAnimation(rig,clip,options);
        CHECK(baked.maximumConservativeWeightError&&*baked.maximumConservativeWeightError<=options.maximumWeightError);
        CHECK(baked.clip.sampleCurve(curveIndex(baked.clip,"fold"),center)>.79);
        for(int i=0;i<=2000;++i)
        {
            const double time=i/2000.;clip.sample(time,pose);
            const auto actual=evaluateObject3DPoseCorrectives(rig,pose).blendShapes;
            for(std::size_t channel=0;channel<actual.size();++channel)
                CHECK(near(baked.clip.sampleCurve(channel,time),actual[channel],
                    *baked.maximumConservativeWeightError+1e-8));
        }
    }
}
void multi()
{
    Object3DEngine engine;const auto object=engine.createObject(fixtureData());
    Object3DPoseCorrectiveProgram rules{{
        {"fold",{{"elbow",Quaternion::fromAxisAngle({1,0,0},std::numbers::pi/2),std::numbers::pi/2}},1},
        {"fold",{{"elbow",Quaternion::fromAxisAngle({1,0,0},-std::numbers::pi/2),std::numbers::pi/2}},.6},
        {"keep",{{"elbow",Quaternion::fromAxisAngle({1,0,0},std::numbers::pi/4),std::numbers::pi/4}},.8}}};
    const auto rig=createObject3DPoseCorrectiveRig(object,rules);
    auto start=object.get().skeleton()->bones()[1].bindTransform,middle=start,end=start;
    middle.rotation=Quaternion::fromAxisAngle({1,0,0},.25);
    end.rotation=Quaternion::fromAxisAngle({1,0,0},std::numbers::pi/2);
    AnimationClip input(object.get().skeleton(),1,{{1,{{0,start},{.4,middle},{1,end}}}});
    const auto result=bakeObject3DPoseCorrectiveAnimation(rig,input);
    CHECK(result.clip.curves().size()==2&&result.clip.tracks()[0].keys.size()==3);
    for(const auto &curve:result.clip.curves())
        CHECK(std::any_of(curve.keys.begin(),curve.keys.end(),[](const auto &key){return key.seconds==.4;}));
    Pose pose(input.skeleton());
    for(int i=0;i<=1000;++i)
    {
        const double time=i/1000.;input.sample(time,pose);
        const auto expected=evaluateObject3DPoseCorrectives(rig,pose);
        for(std::size_t channel=0;channel<expected.blendShapes.size();++channel)
            CHECK(near(result.clip.sampleCurve(channel,time),expected.blendShapes[channel],.0011));
    }
}
#if IIACTOROBJECT_HAS_NSCHARACTER
void human(const std::filesystem::path &directory)
{
    Object3DEngine engine;NSCharacter::MakeHumanTemplateOptions load;load.semanticControls=false;
    const auto character=NSCharacter::loadMakeHumanTemplate(engine.reference(),directory,load);
    const auto generated=character.generate();auto data=generated->data();
    CHECK(data.bones.size()>=150&&data.meshes[0].topology->points.size()>=19'000);
    const auto elbow=generated->skeleton()->findBone("lowerarm01.L");CHECK(elbow!=NoBone);
    auto &topology=*data.meshes[0].topology;std::uint32_t point=NoTopologyIndex;
    for(std::size_t i=0;i<topology.points.size();++i)
        if(std::any_of(topology.points[i].weights.begin(),topology.points[i].weights.end(),
            [&](const auto &weight){return weight.bone==elbow&&weight.weight>.25;}))
        {point=static_cast<std::uint32_t>(i);break;}
    CHECK(point!=NoTopologyIndex);topology.morphTargets.push_back({"elbow_fix_fixture",{{point,{0,0,.005}}}});
    const auto object=engine.createObject(std::move(data));NSCharacter::CharacterAsset asset(object);
    Object3DPoseCorrectiveProgram rules{{{"elbow_fix_fixture",{{"lowerarm01.L",Quaternion::fromAxisAngle({1,0,0},.8),.8}},1}}};
    const auto rig=character.createPoseCorrectiveRig(asset,rules);
    auto start=object.get().skeleton()->bones()[elbow].bindTransform,end=start;
    end.rotation=product(start.rotation,Quaternion::fromAxisAngle({1,0,0},.8));
    const AnimationClip motion(object.get().skeleton(),1,{{elbow,{{0,start},{1,end}}}});
    const auto result=bakeObject3DPoseCorrectiveAnimation(rig,motion);
    CHECK(near(result.clip.sampleCurve(0,.5),.5,1e-7)&&near(result.clip.sampleCurve(0,1),1,1e-7));
}
#endif
void emit(const std::vector<std::uint8_t> &bytes)
{
    constexpr char hex[]="0123456789abcdef";
    for(const auto byte:bytes)std::cout<<hex[byte>>4]<<hex[byte&15];
    std::cout<<'\n';
}
void fixture(std::string_view mode)
{
    Object3DEngine engine;const auto object=engine.createObject(fixtureData());
    const bool narrow=mode.starts_with("narrow_");
    const auto rig=createObject3DPoseCorrectiveRig(object,narrow?program(.3*std::numbers::pi/2,.02):program());
    PoseCorrectiveAnimationOptions options;
    options.maximumSampleInterval=narrow?1:.3;options.maximumWeightError=narrow?.001:.0002;
    const auto result=bakeObject3DPoseCorrectiveAnimation(rig,motion(object.get().skeleton()),options);
    if(mode=="glb_fixture"||mode=="narrow_glb_fixture")
    {
        GlbMorphAnimationOptions morph;morph.matchUnmappedCurvesByName=true;
        emit(exportObject3DGlb(object.get(),result.clip,{},morph));
    }
    else
    {
        FbxAnimationExportOptions fbx;fbx.exportMatchingMorphCurves=true;
        emit(exportObject3DFbx(object.get(),result.clip,fbx));
    }
}
int main(int argc,char **argv)
{
    try
    {
#if IIACTOROBJECT_HAS_NSCHARACTER
        if(argc==3&&std::string_view(argv[1])=="human"){human(argv[2]);return 0;}
#endif
        if(argc!=2)return 2;const std::string_view mode=argv[1];
        if(mode=="accuracy")accuracy();else if(mode=="validation")validation();
        else if(mode=="bound")bound();else if(mode=="narrow")narrow();else if(mode=="multi")multi();
        else if(mode=="conservative")conservative();
        else if(mode=="conservative_paths")conservativePaths();
        else if(mode=="glb_fixture"||mode=="fbx_fixture"||mode=="narrow_glb_fixture"||mode=="narrow_fbx_fixture")fixture(mode);else return 2;
        return 0;
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
