#include <iiActorObject/NSCharacter/HumanAutomaticCorrectives.h>
#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#include <iiActorObject/DualQuaternionSkinning.h>
#include <iiActorObject/Object3DObj.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>

using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}CHECK(failed);}
double dot(Vector3 a,Vector3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vector3 sub(Vector3 a,Vector3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vector3 unit(Vector3 a){const auto n=std::hypot(a.x,a.y,a.z);CHECK(n>0);return {a.x/n,a.y/n,a.z/n};}
Quaternion multiply(Quaternion a,Quaternion b)
{return Quaternion{a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
                   a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z}.normalized();}
Object3DData fixture()
{
    auto data=parseObject3DObj("v 1 0 0\nv 1 1 0\nv 0 1 0\nv 0 0 0\nf 1 2 3 4\n");
    data.kind=Object3DKind::SkeletalMesh;
    data.bones={{"root",NoBone,{}},{"L_hand",5,{{1,0,0}}},{"R_hand",6,{{-1,0,0}}},
        {"L_foot",7,{{0,-1,0}}},{"R_foot",8,{{0,-1,0}}},
        {"L_forearm",0,{{.5,1,0}}},{"R_forearm",0,{{-.5,1,0}}},
        {"L_leg",0,{{.2,1,0}}},{"R_leg",0,{{-.2,1,0}}}};
    for(auto &point:data.meshes[0].topology->points)point.weights={{0,.5},{5,.5}};
    return data;
}
std::vector<HumanJointRule> rules()
{
    return {{"LeftForeArm","L_forearm"},{"LeftHand","L_hand"},
            {"RightForeArm","R_forearm"},{"RightHand","R_hand"},
            {"LeftLeg","L_leg"},{"LeftFoot","L_foot"},
            {"RightLeg","R_leg"},{"RightFoot","R_foot"}};
}
void plan()
{
    Object3DEngine engine;auto data=fixture();data.bones[5].bindTransform.rotation=Quaternion::fromAxisAngle({0,1,0},.3);
    const auto object=engine.createObject(std::move(data));const CharacterAsset character(object);
    const auto mapping=mapHumanJoints(character,rules());const auto planned=planHumanAutomaticCorrectives(mapping);
    CHECK(planned.object==object&&planned.program.samples.size()==4&&planned.skipped.empty());
    const auto &skeleton=object.get().skeleton();Pose baseline(skeleton);const auto original=baseline.worldMatrices();
    for(const auto &sample:planned.program.samples)
    {
        const auto &condition=sample.conditions[0];const auto bone=skeleton->findBone(condition.joint);
        const auto role=sample.morph.substr(std::string("AutoDQ_").size());
        const auto descendant=mapping.joint(role.find("ForeArm")!=std::string::npos?
            role.substr(0,role.find("ForeArm"))+"Hand":role.substr(0,role.find("Leg"))+"Foot");
        CHECK(bone!=NoBone&&descendant!=NoBone);
        const auto from=unit(sub(original[descendant].transformPoint({}),original[bone].transformPoint({})));
        Pose moved(skeleton);auto local=moved.locals()[bone];local.rotation=multiply(local.rotation,condition.rotation);moved.setLocal(bone,local);
        const auto world=moved.worldMatrices();const auto to=unit(sub(world[descendant].transformPoint({}),world[bone].transformPoint({})));
        const Vector3 wanted=role.find("ForeArm")!=std::string::npos?Vector3{0,1,0}:Vector3{0,0,-1};
        CHECK(dot(to,wanted)>dot(from,wanted)+.2);
    }
    const auto generated=createHumanAutomaticCorrectives(planned);
    CHECK(generated.object.valid()&&generated.object!=object&&generated.program.samples.size()==4);
    CHECK(generated.report.positionDeltas>0);
}
void validation()
{
    Object3DEngine engine;const auto object=engine.createObject(fixture());const CharacterAsset character(object);
    const auto mapping=mapHumanJoints(character,rules());auto options=HumanAutomaticCorrectiveOptions{};
    options.forward=options.up;rejects([&]{(void)planHumanAutomaticCorrectives(mapping,options);});
    options={};options.radiusRadians=options.elbowRadians;rejects([&]{(void)planHumanAutomaticCorrectives(mapping,options);});
    options={};options.samplesPerJoint=0;rejects([&]{(void)planHumanAutomaticCorrectives(mapping,options);});
    options.samplesPerJoint=33;rejects([&]{(void)planHumanAutomaticCorrectives(mapping,options);});
    options={};options.samplesPerJoint=3;options.radiusRadians=2;
    const auto limited=planHumanAutomaticCorrectives(mapping,options);
    CHECK(limited.program.samples.size()==12);
    for(const auto &sample:limited.program.samples)CHECK(std::abs(sample.conditions[0].radiusRadians-.4)<1e-12);
    options={};auto partialRules=rules();partialRules.resize(2);const auto partial=mapHumanJoints(character,partialRules);
    const auto planned=planHumanAutomaticCorrectives(partial);CHECK(planned.program.samples.size()==1&&planned.skipped.size()==3);
    options.skipMissingRoles=false;rejects([&]{(void)planHumanAutomaticCorrectives(partial,options);});
    auto ambiguous=fixture();ambiguous.bones[5].bindTransform.scale={1,2,1};
    const auto different=engine.createObject(std::move(ambiguous));const CharacterAsset other(different);
    const auto otherMapping=mapHumanJoints(other,rules());options={};const auto skipped=planHumanAutomaticCorrectives(otherMapping,options);
    CHECK(skipped.program.samples.size()==3&&skipped.skipped.size()==1);
    options.skipAmbiguousAxes=false;rejects([&]{(void)planHumanAutomaticCorrectives(otherMapping,options);});
    std::vector<Transform> baseline;for(const auto &bone:object.get().skeleton()->bones())baseline.push_back(bone.bindTransform);
    baseline[5].rotation=Quaternion::fromAxisAngle({1,0,0},.2);options={};options.baselineLocals=baseline;
    const auto custom=planHumanAutomaticCorrectives(mapping,options);auto wrong=custom.baselineLocals;wrong[5].position.x+=.1;
    AutomaticCorrectiveOptions bake;bake.baselineLocals=wrong;
    rejects([&]{(void)createHumanAutomaticCorrectives(custom,bake);});
    CHECK(createHumanAutomaticCorrectives(custom).object.valid());
    options={};auto invalid=planned;CHECK(engine.eraseObject(object.id()));
    rejects([&]{(void)createHumanAutomaticCorrectives(invalid);});
}
void interpolation()
{
    Object3DEngine engine;const auto object=engine.createObject(fixture());const CharacterAsset character(object);
    const auto mapping=mapHumanJoints(character,rules());auto options=HumanAutomaticCorrectiveOptions{};
    options.knees=false;options.samplesPerJoint=1;
    const auto one=planHumanAutomaticCorrectives(mapping,options);
    options.samplesPerJoint=3;const auto multiple=planHumanAutomaticCorrectives(mapping,options);
    CHECK(one.program.samples.size()==2&&multiple.program.samples.size()==6);
    for(std::size_t i=0;i<3;++i)
    {
        CHECK(multiple.program.samples[i].morph=="AutoDQ_LeftForeArm_"+std::to_string(i+1));
        CHECK(std::abs(multiple.program.samples[i].conditions[0].radiusRadians-.4)<1e-12);
    }
    const auto a=createHumanAutomaticCorrectives(one),b=createHumanAutomaticCorrectives(multiple);
    const auto rigA=createObject3DPoseCorrectiveRig(a.object,a.program);
    const auto rigB=createObject3DPoseCorrectiveRig(b.object,b.program);
    auto source=object.get().instantiate(),single=a.object.get().instantiate(),dense=b.object.get().instantiate();
    auto component=[](iiCharacter &actor){for(auto *part:actor.components())
        if(auto *mesh=dynamic_cast<SkeletalMeshComponent*>(part))return mesh;return static_cast<SkeletalMeshComponent*>(nullptr);};
    const auto original=component(*source),first=component(*single),second=component(*dense);
    CHECK(original&&first&&second);const auto &mesh=*original->mesh();
    std::vector<double> factors(mesh.lods()[0].vertices.size(),1);
    std::vector<DeformedVertex> expected(factors.size());double worstSingle=0,worstDense=0;
    for(std::size_t i=0;i<=24;++i)
    {
        const auto rotation=Quaternion::fromAxisAngle({0,0,1},1.2*static_cast<double>(i)/24);
        for(auto *actor:{source.get(),single.get(),dense.get()})
        {auto local=actor->pose().locals()[5];local.rotation=rotation;actor->pose().setLocal(5,local);}
        applyObject3DPoseCorrectives(rigA,*single);applyObject3DPoseCorrectives(rigB,*dense);
        deformDualQuaternionSkin(mesh,source->pose(),0,original->morphWeights(),factors,expected);
        const auto singleVertices=first->deformedVertices(),denseVertices=second->deformedVertices();
        for(std::size_t v=0;v<expected.size();++v)
        {
            const auto error=[](Vector3 x,Vector3 y){return std::hypot(x.x-y.x,x.y-y.y,x.z-y.z);};
            const auto singleError=error(singleVertices[v].position,expected[v].position);
            const auto denseError=error(denseVertices[v].position,expected[v].position);
            worstSingle=std::max(worstSingle,singleError);worstDense=std::max(worstDense,denseError);
            if(i%8==0)CHECK(denseError<1e-8);
        }
    }
    CHECK(worstSingle>1e-4&&worstDense<worstSingle*.65);
    std::cout<<"Maximum sampled position error: one="<<worstSingle<<" three="<<worstDense<<'\n';
}
void human(const std::filesystem::path &directory,std::size_t samplesPerJoint)
{
    Object3DEngine engine;MakeHumanTemplateOptions options;options.semanticControls=false;
    const auto human=loadMakeHumanTemplate(engine.reference(),directory,options);const auto character=human.generate();
    CHECK(character->data().bones.size()>=150&&character->data().meshes[0].topology->points.size()>=19'000);
    const auto mapping=mapHumanJoints(*character,humanJointRules(HumanSkeletonConvention::MakeHuman));
    HumanAutomaticCorrectiveOptions sampling;sampling.samplesPerJoint=samplesPerJoint;
    const auto planned=planHumanAutomaticCorrectives(mapping,sampling);
    CHECK(planned.program.samples.size()>=2*samplesPerJoint);
    const auto generated=createHumanAutomaticCorrectives(planned);CHECK(generated.report.positionDeltas>0);
    auto before=character->instantiate(),after=generated.object.get().instantiate();
    const auto &condition=planned.program.samples[0].conditions[0];const auto bone=character->skeleton()->findBone(condition.joint);
    auto a=before->pose().locals()[bone],b=after->pose().locals()[bone];
    a.rotation=multiply(character->skeleton()->bones()[bone].bindTransform.rotation,condition.rotation);
    b.rotation=a.rotation;before->pose().setLocal(bone,a);after->pose().setLocal(bone,b);
    const auto rig=createObject3DPoseCorrectiveRig(generated.object,generated.program);applyObject3DPoseCorrectives(rig,*after);
    auto components=[](iiCharacter &actor){std::vector<SkeletalMeshComponent*> result;for(auto *p:actor.components())
        if(auto *mesh=dynamic_cast<SkeletalMeshComponent*>(p))result.push_back(mesh);return result;};
    const auto aParts=components(*before),bParts=components(*after);CHECK(!aParts.empty()&&aParts.size()==bParts.size());
    const auto &mesh=*aParts[0]->mesh();std::vector<double> factors(mesh.lods()[0].vertices.size(),1);
    std::vector<DeformedVertex> expected(factors.size());
    deformDualQuaternionSkin(mesh,before->pose(),0,aParts[0]->morphWeights(),factors,expected);
    const auto actual=bParts[0]->deformedVertices();CHECK(actual.size()==expected.size());
    for(std::size_t i=0;i<actual.size();++i)
    {
        CHECK(std::hypot(actual[i].position.x-expected[i].position.x,actual[i].position.y-expected[i].position.y,
                         actual[i].position.z-expected[i].position.z)<1e-7);
        CHECK(std::hypot(actual[i].normal.x-expected[i].normal.x,actual[i].normal.y-expected[i].normal.y,
                         actual[i].normal.z-expected[i].normal.z)<1e-7);
    }
}
int main(int argc,char **argv)
{
    try{if(argc==3&&std::string_view(argv[1])=="human"){human(argv[2],1);return 0;}
        if(argc==3&&std::string_view(argv[1])=="human_dense"){human(argv[2],3);return 0;}if(argc!=2)return 2;
        const std::string_view mode=argv[1];if(mode=="plan")plan();else if(mode=="validation")validation();
        else if(mode=="interpolation")interpolation();else return 2;return 0;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
