#include <iiActorObject/NSCharacter/HumanPosePlanner.h>
#include <iiActorObject/NSCharacter/HumanoidGenerator.h>
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DPoseSnapshot.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}CHECK(rejected);}
Vector3 sub(Vector3 a,Vector3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vector3 unit(Vector3 a){const auto n=std::hypot(a.x,a.y,a.z);CHECK(n>0);return {a.x/n,a.y/n,a.z/n};}
Vector3 cross(Vector3 a,Vector3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
void same(Vector3 a,Vector3 b){CHECK(std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)<3e-9);}
auto source(Object3DEngine &engine){return HumanoidGenerator::generate(engine.reference(),{}, {36,false,false});}
void verify(const HumanJointMapping &mapping,const HumanPosePlan &plan,const Object3DAsset &asset)
{
    auto actor=asset.instantiate();const auto world=actor->pose().worldMatrices();
    const auto point=[&](const std::string &role){const auto i=mapping.joint(role);CHECK(i!=NoBone);return world[i].transformPoint({});};
    for(const auto &direction:plan.directions)
    {
        auto target=Vector3{};
        if(direction.descendantBones.empty())target=point(direction.descendantRole);
        else
        {
            for(const auto bone:direction.descendantBones)
            {
                CHECK(bone<world.size());const auto p=world[bone].transformPoint({});target.x+=p.x;target.y+=p.y;target.z+=p.z;
            }
            const auto count=static_cast<double>(direction.descendantBones.size());target.x/=count;target.y/=count;target.z/=count;
        }
        same(unit(sub(target,point(direction.jointRole))),unit(direction.objectDirection));
    }
    for(const auto &frame:plan.frames)
    {
        const auto first=unit(sub(point(frame.primaryDescendantRole),point(frame.jointRole)));
        const auto second=unit(sub(point(frame.planeDescendantRole),point(frame.jointRole)));
        same(first,unit(frame.objectDirection));same(unit(cross(first,second)),unit(frame.objectPlaneNormal));
    }
    for(const std::string side:{"Left","Right"})for(const std::string finger:{"Thumb","Index","Middle","Ring","Pinky"})
    {
        const auto prefix=side+"Hand"+finger;
        same(unit(sub(point(prefix+"2"),point(prefix+"1"))),unit(sub(point(prefix+"3"),point(prefix+"2"))));
    }
}
void basic()
{
    Object3DEngine engine;auto original=source(engine);const auto originalMapping=mapHumanJoints(*original,humanJointRules(HumanSkeletonConvention::Procedural));
    auto bent=original->data();const auto index=originalMapping.joint("LeftHandIndex2"),middle=originalMapping.joint("RightHandMiddle2");
    bent.bones[index].bindTransform.rotation=Quaternion::fromAxisAngle({0,0,1},.45);
    bent.bones[middle].bindTransform.rotation=Quaternion::fromAxisAngle({0,1,0},.4);
    auto character=std::make_shared<CharacterAsset>(engine.createObject(std::move(bent)));
    const auto mapping=mapHumanJoints(*character,humanJointRules(HumanSkeletonConvention::Procedural));
    const auto beforeActor=character->instantiate();const auto before=beforeActor->pose().worldMatrices();
    const auto a=before[mapping.joint("LeftHandIndex1")].transformPoint({});const auto b=before[index].transformPoint({});
    const auto c=before[mapping.joint("LeftHandIndex3")].transformPoint({});
    const auto bend=sub(unit(sub(b,a)),unit(sub(c,b)));CHECK(std::hypot(bend.x,bend.y,bend.z)>.1);
    const auto count=engine.objectCount();const auto plan=planHumanReferencePose(mapping);
    CHECK(engine.objectCount()==count&&!character->data().referencePose&&plan.skipped.empty());
    CHECK(plan.directions.size()==20&&plan.frames.size()==2&&plan.order.size()==22);
    CharacterEditor editor(character);setHumanOrderedReferencePose(editor,mapping,plan.directions,plan.frames,plan.order,plan.referenceOptions);
    verify(mapping,plan,editor.asset()->object().get());CHECK(engine.reference().owns(editor.asset()->object()));
    const auto snapshot=createReferenceObject3DPoseSnapshot(editor.asset()->object());verify(mapping,plan,snapshot.get());
}
void axes()
{
    Object3DEngine engine;auto character=source(engine);const auto mapping=mapHumanJoints(*character,humanJointRules(HumanSkeletonConvention::Procedural));
    HumanPosePlanOptions options;options.left={-1,0,0};options.up={0,0,1};options.forward={0,1,0};
    const auto plan=planHumanReferencePose(mapping,options);CharacterEditor editor(character);
    setHumanOrderedReferencePose(editor,mapping,plan.directions,plan.frames,plan.order,plan.referenceOptions);verify(mapping,plan,editor.asset()->object().get());
}
void finger4()
{
    Object3DEngine engine;auto original=source(engine);const auto initial=mapHumanJoints(*original,humanJointRules(HumanSkeletonConvention::Procedural));
    auto data=original->data();const auto third=initial.joint("LeftHandIndex3");
    data.bones[third].bindTransform.rotation=Quaternion::fromAxisAngle({0,0,1},.35);
    data.bones.push_back({"index_04_l",third,{{.015,0,0}}});auto character=std::make_shared<CharacterAsset>(engine.createObject(std::move(data)));
    const auto defaults=humanJointRules(HumanSkeletonConvention::Procedural);std::vector<HumanJointRule> rules(defaults.begin(),defaults.end());
    rules.push_back({"LeftHandIndex4","index_04_l",HumanJointSource::BoneName});const auto mapping=mapHumanJoints(*character,rules);
    const auto plan=planHumanReferencePose(mapping);CHECK(plan.directions.size()==21&&plan.order.size()==23);
    CharacterEditor editor(character);setHumanOrderedReferencePose(editor,mapping,plan.directions,plan.frames,plan.order,plan.referenceOptions);
    verify(mapping,plan,editor.asset()->object().get());const auto actor=editor.asset()->instantiate();const auto world=actor->pose().worldMatrices();
    const auto p=[&](std::string_view role){return world[mapping.joint(role)].transformPoint({});};
    same(unit(sub(p("LeftHandIndex2"),p("LeftHandIndex1"))),unit(sub(p("LeftHandIndex4"),p("LeftHandIndex3"))));
}
void partial()
{
    Object3DEngine engine;auto character=source(engine);std::vector<HumanJointRule> rules;
    for(const auto &rule:humanJointRules(HumanSkeletonConvention::Procedural))
        if(rule.role=="LeftArm"||rule.role=="LeftForeArm"||rule.role=="LeftHand")rules.push_back(rule);
    const auto mapping=mapHumanJoints(*character,rules);HumanPosePlanOptions options;
    options.legs=false;options.feet=false;options.hands=false;options.fingers=false;
    options.skipMissingRoles=false;
    rejects([&]{(void)planHumanReferencePose(mapping,options);});
    options.skipMissingRoles=true;const auto plan=planHumanReferencePose(mapping,options);
    CHECK(plan.directions.size()==2&&plan.frames.empty()&&plan.skipped.size()==2&&plan.order.size()==2);
}
void failures()
{
    Object3DEngine engine;auto character=source(engine);const auto mapping=mapHumanJoints(*character,humanJointRules(HumanSkeletonConvention::Procedural));
    const auto count=engine.objectCount();HumanPosePlanOptions options;options.left={};rejects([&]{(void)planHumanReferencePose(mapping,options);});
    options={};options.forward=options.left;rejects([&]{(void)planHumanReferencePose(mapping,options);});
    options={};options.up.x=std::numeric_limits<double>::quiet_NaN();rejects([&]{(void)planHumanReferencePose(mapping,options);});
    options={};options.referenceOptions.maximumConstraints=1;rejects([&]{(void)planHumanReferencePose(mapping,options);});
    options={};options.referenceOptions.aiming.maximumAncestorVisits=0;rejects([&]{(void)planHumanReferencePose(mapping,options);});
    CHECK(engine.objectCount()==count&&!character->data().referencePose);
}
void lifetime()
{
    std::optional<HumanJointMapping> mapping;std::optional<HumanPosePlan> plan;
    {Object3DEngine engine;auto character=source(engine);mapping=mapHumanJoints(*character,humanJointRules(HumanSkeletonConvention::Procedural));plan=planHumanReferencePose(*mapping);}
    CHECK(!mapping->valid()&&plan->directions.size()==20);rejects([&]{(void)planHumanReferencePose(*mapping);});
}
void toeGroup()
{
    Object3DEngine engine;auto original=source(engine);
    const auto initial=mapHumanJoints(*original,humanJointRules(HumanSkeletonConvention::Procedural));
    auto data=original->data();const auto foot=initial.joint("LeftFoot");CHECK(foot!=NoBone);
    auto rules=std::vector<HumanJointRule>(humanJointRules(HumanSkeletonConvention::Procedural).begin(),
                                           humanJointRules(HumanSkeletonConvention::Procedural).end());
    for(auto &rule:rules)if(rule.role=="LeftToeBase")rule.source="absent-toe-base";
    for(unsigned n=1;n<=4;++n)data.bones.push_back({"toe"+std::to_string(n)+"-1.L",foot,{{.01*n,0,.1}}});
    auto partial=std::make_shared<CharacterAsset>(engine.createObject(data));auto mapping=mapHumanJoints(*partial,rules);
    const auto skipped=planHumanReferencePose(mapping);CHECK(skipped.skipped.size()==1);
    HumanPosePlanOptions strict;strict.skipMissingRoles=false;rejects([&]{(void)planHumanReferencePose(mapping,strict);});
    data.bones.push_back({"toe5-1.L",foot,{{.05,0,.1}}});auto complete=std::make_shared<CharacterAsset>(engine.createObject(data));
    mapping=mapHumanJoints(*complete,rules);const auto plan=planHumanReferencePose(mapping,strict);CHECK(plan.skipped.empty());
    CharacterEditor editor(complete);setHumanOrderedReferencePose(editor,mapping,plan.directions,plan.frames,plan.order,plan.referenceOptions);
    verify(mapping,plan,editor.asset()->object().get());
    const auto left=std::find_if(plan.directions.begin(),plan.directions.end(),[](const auto &d){return d.jointRole=="LeftFoot";});
    CHECK(left!=plan.directions.end());const std::array oneGroup{*left};const auto standalone=createHumanPoseSnapshot(mapping,oneGroup);
    const auto actor=standalone.instantiate();const auto world=actor->pose().worldMatrices();const auto centerFoot=world[foot].transformPoint({});Vector3 center{};
    for(auto bone:left->descendantBones){const auto p=world[bone].transformPoint({});center.x+=p.x;center.y+=p.y;center.z+=p.z;}
    center.x/=5;center.y/=5;center.z/=5;same(unit(sub(center,centerFoot)),{0,0,1});
    data.bones.back().parent=initial.joint("RightFoot");auto wrong=std::make_shared<CharacterAsset>(engine.createObject(data));
    auto bad=mapHumanJoints(*wrong,rules);rejects([&]{(void)planHumanReferencePose(bad);});
    data=complete->data();data.semantics.push_back({"toe1-1.L",initial.joint("RightFoot")});
    auto ambiguous=std::make_shared<CharacterAsset>(engine.createObject(std::move(data)));
    bad=mapHumanJoints(*ambiguous,rules);rejects([&]{(void)planHumanReferencePose(bad);});
}
void human(const std::filesystem::path &path)
{
    std::ifstream stream(path,std::ios::binary|std::ios::ate);const auto size=stream.tellg();CHECK(size>0&&size<256*1024*1024);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));stream.seekg(0);CHECK(bool(stream.read(reinterpret_cast<char*>(bytes.data()),size)));
    Object3DEngine engine;auto character=std::make_shared<CharacterAsset>(importObject3DFbx(engine.reference(),bytes).object);
    const auto mapping=mapHumanJoints(*character,humanJointRules(HumanSkeletonConvention::MakeHuman));const auto plan=planHumanReferencePose(mapping);
    CHECK(plan.skipped.empty()&&plan.directions.size()==20&&plan.frames.size()==2);
    for(const std::string side:{"Left","Right"})
    {
        const auto aim=std::find_if(plan.directions.begin(),plan.directions.end(),[&](const auto &x){return x.jointRole==side+"Foot";});
        CHECK(aim!=plan.directions.end()&&aim->descendantRole.empty()&&aim->descendantBones.size()==5);
        for(unsigned i=0;i<5;++i)
            CHECK(character->data().bones[aim->descendantBones[i]].name=="toe"+std::to_string(i+1)+"-1"+(side=="Left"?".L":".R"));
    }
    CharacterEditor editor(character);setHumanOrderedReferencePose(editor,mapping,plan.directions,plan.frames,plan.order,plan.referenceOptions);
    verify(mapping,plan,editor.asset()->object().get());CHECK(character->data().bones.size()==163);
    const auto projectBytes=exportObject3DProject(editor.session());CHECK(projectBytes[8]==30);
    Object3DEngine recovered;const auto saved=importObject3DProject(recovered.reference(),projectBytes);
    CHECK(saved.editor.get().object().get().data().referencePose==editor.asset()->data().referencePose);
    const auto snapshot=createReferenceObject3DPoseSnapshot(editor.asset()->object());verify(mapping,plan,snapshot.get());
    const auto fbx=exportObject3DFbx(snapshot.get(),FbxExportOptions{.allowMorphNormalMetadata=true});
    const auto glb=exportObject3DGlb(snapshot.get());Object3DEngine exchanged;
    const std::array outputs{importObject3DFbx(exchanged.reference(),fbx).object,importObject3DGlb(exchanged.reference(),glb).object};
    for(const auto &output:outputs)
    {
        const auto map=mapHumanJoints(CharacterAsset(output),humanJointRules(HumanSkeletonConvention::MakeHuman));
        CHECK(map.completeBody());
        const auto actor=output.get().instantiate();const auto world=actor->pose().worldMatrices();
        for(const std::string side:{"Left","Right"})
        {
            const auto foot=world[map.joint(side+"Foot")].transformPoint({});Vector3 center{};
            for(unsigned i=1;i<=5;++i)
            {
                const auto bone=output.get().skeleton()->findBone("toe"+std::to_string(i)+"-1"+(side=="Left"?".L":".R"));
                CHECK(bone!=NoBone);const auto point=world[bone].transformPoint({});center.x+=point.x;center.y+=point.y;center.z+=point.z;
            }
            center.x/=5;center.y/=5;center.z/=5;const auto direction=unit(sub(center,foot));
            CHECK(std::hypot(direction.x,direction.y,direction.z-1)<2e-4);
        }
    }
}
int main(int argc,char **argv)
{
    try{if(argc<2)return 2;const std::string_view mode=argv[1];
        if(mode=="basic")basic();else if(mode=="axes")axes();else if(mode=="finger4")finger4();else if(mode=="partial")partial();else if(mode=="failures")failures();else if(mode=="lifetime")lifetime();else if(mode=="toe_group")toeGroup();else if(mode=="human"&&argc==3)human(argv[2]);else return 2;
        std::cout<<mode<<" passed\n";return 0;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
