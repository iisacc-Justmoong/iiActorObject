#include <iiActorObject/NSCharacter/HumanPosePlanner.h>
#include <iiActorObject/NSCharacter/HumanPoseRetarget.h>
#include <iiActorObject/NSCharacter/HumanJointNaming.h>
#include <iiActorObject/NSCharacter/HumanoidGenerator.h>
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#include <iiActorObject/Object3DProject.h>
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
std::vector<std::pair<std::string,std::string>> expectedRoles()
{
    std::vector<std::pair<std::string,std::string>> out{{"Hips","CC_Base_Hip"},{"Spine","CC_Base_Waist"},
        {"Spine1","CC_Base_Spine01"},{"Spine2","CC_Base_Spine02"},{"Neck","CC_Base_NeckTwist01"},
        {"Neck1","CC_Base_NeckTwist02"},{"Head","CC_Base_Head"}};
    for(const auto &[side,code]:std::array{std::pair{"Left","L"},std::pair{"Right","R"}})
    {
        const auto add=[&](std::string role,std::string name){out.emplace_back(std::string(side)+role,"CC_Base_"+std::string(code)+"_"+name);};
        add("Shoulder","Clavicle");add("Arm","Upperarm");add("ForeArm","Forearm");add("Hand","Hand");
        add("UpLeg","Thigh");add("Leg","Calf");add("Foot","Foot");add("ToeBase","ToeBase");
        for(const auto &[role,name]:std::array{std::pair{"Thumb","Thumb"},std::pair{"Index","Index"},
            std::pair{"Middle","Mid"},std::pair{"Ring","Ring"},std::pair{"Pinky","Pinky"}})
            for(unsigned n=1;n<=3;++n)add("Hand"+std::string(role)+std::to_string(n),std::string(name)+std::to_string(n));
    }
    return out;
}
struct Fixture final{std::shared_ptr<const CharacterAsset> original;std::shared_ptr<CharacterAsset> creator;HumanJointMapping originalMapping;};
Fixture fixture(Object3DEngine &engine,bool missing=false,bool badHierarchy=false)
{
    auto original=HumanoidGenerator::generate(engine.reference(),{}, {36,false,false});
    auto oldMapping=mapHumanJoints(*original,humanJointRules(HumanSkeletonConvention::Procedural));auto data=original->data();
    for(const auto &[role,name]:expectedRoles())
    {
        const auto id=oldMapping.joint(role);CHECK(id!=NoBone);data.bones[id].name=name;
    }
    if(missing)data.bones[oldMapping.joint("LeftHandIndex2")].name="missing-index-bone";
    if(badHierarchy)data.bones[oldMapping.joint("LeftForeArm")].parent=0;
    auto creator=std::make_shared<CharacterAsset>(engine.createObject(std::move(data)));
    return {std::move(original),std::move(creator),std::move(oldMapping)};
}
void mapping()
{
    Object3DEngine engine;auto f=fixture(engine);const auto rules=humanJointRules(HumanSkeletonConvention::CharacterCreatorG3);
    const auto cc=mapHumanJoints(*f.creator,rules);CHECK(cc.completeBody());
    const auto detected=detectHumanSkeletonConvention(*f.creator);
    CHECK(detected.status==HumanConventionDetectionStatus::Unique&&detected.mapping);
    CHECK(detected.completeConventions==std::vector{HumanSkeletonConvention::CharacterCreatorG3});
    CHECK(cc.bindings().size()==expectedRoles().size());
    for(const auto &[role,name]:expectedRoles())
    {const auto id=cc.joint(role);CHECK(id==f.originalMapping.joint(role)&&f.creator->data().bones[id].name==name);}
    CHECK(cc.joint("LeftHandMiddle1")!=NoBone&&cc.joint("RightToeBase")!=NoBone);
    CHECK(f.creator->data().bones[cc.joint("LeftHandMiddle1")].name=="CC_Base_L_Mid1");
    const auto plan=planHumanReferencePose(cc);CHECK(plan.skipped.empty()&&plan.directions.size()==20&&plan.frames.size()==2);
    CharacterEditor editor(f.creator);setHumanOrderedReferencePose(editor,cc,plan.directions,plan.frames,plan.order,plan.referenceOptions);
    CHECK(engine.reference().owns(editor.asset()->object())&&editor.asset()->data().referencePose);
    const auto retarget=planHumanPoseRetarget(cc,f.originalMapping);CHECK(!retarget.joints.empty());
    Pose sourceReference(f.creator->object().get().skeleton()),sourcePose(f.creator->object().get().skeleton()),targetReference(f.original->object().get().skeleton());
    auto arm=sourcePose.locals()[cc.joint("LeftArm")];arm.rotation=Quaternion::fromAxisAngle({0,0,1},.4);sourcePose.setLocal(cc.joint("LeftArm"),arm);
    const auto result=retargetPose(sourceReference,sourcePose,targetReference,retarget.joints);CHECK(result.size()==targetReference.locals().size());
    const auto bytes=exportCc5BoneMap(cc);const std::string text(bytes.begin(),bytes.end());CHECK(text.find("CC_Base_L_Hand = LeftHand")!=std::string::npos);
}
void partial()
{
    Object3DEngine engine;auto f=fixture(engine,true);const auto cc=mapHumanJoints(*f.creator,humanJointRules(HumanSkeletonConvention::CharacterCreatorG3));
    CHECK(cc.completeBody()&&cc.joint("LeftHandIndex2")==NoBone);
    const auto plan=planHumanReferencePose(cc);CHECK(plan.skipped.size()==1&&plan.skipped[0].find("LeftHandIndex")!=std::string::npos);
    HumanPosePlanOptions options;options.skipMissingRoles=false;rejects([&]{(void)planHumanReferencePose(cc,options);});
}
void hierarchy()
{
    Object3DEngine engine;auto f=fixture(engine,false,true);const auto count=engine.objectCount();
    rejects([&]{(void)mapHumanJoints(*f.creator,humanJointRules(HumanSkeletonConvention::CharacterCreatorG3));});CHECK(engine.objectCount()==count);
}
void naming()
{
    Object3DEngine engine;auto original=HumanoidGenerator::generate(engine.reference(),{}, {36,false,false});
    const auto source=mapHumanJoints(*original,humanJointRules(HumanSkeletonConvention::Procedural));
    const auto plan=planHumanJointNames(source,HumanSkeletonConvention::CharacterCreatorG3);
    CHECK(plan.skipped.empty()&&plan.changes.size()==53);CharacterEditor editor(original);const auto revision=editor.revision();
    CHECK(applyHumanJointNames(editor,source,plan)&&editor.revision()==revision+1);
    const auto renamed=mapHumanJoints(*editor.asset(),humanJointRules(HumanSkeletonConvention::CharacterCreatorG3));CHECK(renamed.completeBody());
    for(const auto &[role,name]:expectedRoles())
    {const auto id=renamed.joint(role);CHECK(id==source.joint(role)&&editor.asset()->data().bones[id].name==name);}
    CHECK(original->data().bones[source.joint("LeftArm")].name!="CC_Base_L_Upperarm");
    const auto bytes=exportObject3DProject(editor.session());Object3DEngine target;const auto restored=importObject3DProject(target.reference(),bytes);
    CHECK(restored.editor.get().object().get().data().bones[renamed.joint("LeftArm")].name=="CC_Base_L_Upperarm");
    CHECK(editor.undo()&&editor.asset()->object()==original->object());CHECK(editor.redo());
    const auto current=editor.asset()->object();const auto before=editor.revision();rejects([&]{(void)applyHumanJointNames(editor,source,plan);});
    CHECK(editor.asset()->object()==current&&editor.revision()==before);
    const auto back=planHumanJointNames(renamed,HumanSkeletonConvention::Procedural);
    CHECK(back.changes.size()==53&&back.skipped.size()==2&&applyHumanJointNames(editor,renamed,back));
    for(std::size_t i=0;i<original->data().bones.size();++i)CHECK(editor.asset()->data().bones[i].name==original->data().bones[i].name);
}
void namingInvalid()
{
    Object3DEngine engine;auto original=HumanoidGenerator::generate(engine.reference(),{}, {36,false,false});
    const auto source=mapHumanJoints(*original,humanJointRules(HumanSkeletonConvention::Procedural));CharacterEditor editor(original);
    const std::array duplicateRole{HumanJointNameRule{"LeftArm","a"},HumanJointNameRule{"LeftArm","b"}};
    rejects([&]{(void)planHumanJointNames(source,duplicateRole);});
    const std::array missing{HumanJointNameRule{"NotMapped","a"}};const auto partial=planHumanJointNames(source,missing);
    CHECK(partial.changes.empty()&&partial.skipped.size()==1);HumanJointNamingOptions options;options.skipMissingRoles=false;
    rejects([&]{(void)planHumanJointNames(source,missing,options);});
    options={};options.maximumRules=0;rejects([&]{(void)planHumanJointNames(source,missing,options);});
    options.maximumRules=1;rejects([&]{(void)planHumanJointNames(source,duplicateRole,options);});
    const std::array emptyName{HumanJointNameRule{"LeftArm",""}};rejects([&]{(void)planHumanJointNames(source,emptyName);});
    const std::array collision{HumanJointNameRule{"LeftArm","duplicate"},HumanJointNameRule{"LeftForeArm","duplicate"}};
    const auto plan=planHumanJointNames(source,collision);const auto revision=editor.revision();const auto count=engine.objectCount();
    rejects([&]{(void)applyHumanJointNames(editor,source,plan);});CHECK(editor.revision()==revision&&editor.asset()->object()==original->object()&&engine.objectCount()==count);
    Object3DEngine other;auto foreign=HumanoidGenerator::generate(other.reference(),{}, {36,false,false});
    const auto otherMapping=mapHumanJoints(*foreign,humanJointRules(HumanSkeletonConvention::Procedural));const auto otherPlan=planHumanJointNames(otherMapping,collision);
    rejects([&]{(void)applyHumanJointNames(editor,source,otherPlan);});CHECK(editor.revision()==revision&&editor.asset()->object()==original->object());
    auto conflicting=original->data();conflicting.semantics.push_back({"CC_Base_L_Upperarm",source.joint("RightArm")});
    auto unsafe=std::make_shared<CharacterAsset>(engine.createObject(std::move(conflicting)));
    const auto unsafeMapping=mapHumanJoints(*unsafe,humanJointRules(HumanSkeletonConvention::Procedural));const auto unsafeCount=engine.objectCount();
    rejects([&]{(void)planHumanJointNames(unsafeMapping,HumanSkeletonConvention::CharacterCreatorG3);});CHECK(engine.objectCount()==unsafeCount);
    const std::array safeRule{HumanJointNameRule{"LeftArm","new_arm_name"}};auto altered=planHumanJointNames(unsafeMapping,safeRule);
    altered.changes[0].name="CC_Base_L_Upperarm";CharacterEditor unsafeEditor(unsafe);const auto unsafeRevision=unsafeEditor.revision();
    rejects([&]{(void)applyHumanJointNames(unsafeEditor,unsafeMapping,altered);});
    CHECK(unsafeEditor.revision()==unsafeRevision&&unsafeEditor.asset()->object()==unsafe->object());
    altered.changes.resize(unsafe->data().bones.size()+1);rejects([&]{(void)applyHumanJointNames(unsafeEditor,unsafeMapping,altered);});
    CHECK(unsafeEditor.revision()==unsafeRevision&&unsafeEditor.asset()->object()==unsafe->object());
    std::optional<HumanJointMapping> expiredMapping;std::optional<HumanJointNamePlan> expiredPlan;
    {Object3DEngine temporary;auto value=HumanoidGenerator::generate(temporary.reference(),{}, {36,false,false});
     expiredMapping=mapHumanJoints(*value,humanJointRules(HumanSkeletonConvention::Procedural));expiredPlan=planHumanJointNames(*expiredMapping,collision);}
    CHECK(!expiredMapping->valid()&&!expiredPlan->source.valid());rejects([&]{(void)planHumanJointNames(*expiredMapping,collision);});
}
void namingAsset(const std::filesystem::path &path)
{
    std::ifstream input(path,std::ios::binary|std::ios::ate);const auto size=input.tellg();CHECK(size>0&&size<256*1024*1024);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));input.seekg(0);CHECK(bool(input.read(reinterpret_cast<char*>(bytes.data()),size)));
    Object3DEngine engine;auto original=std::make_shared<CharacterAsset>(importObject3DFbx(engine.reference(),bytes).object);
    const auto source=mapHumanJoints(*original,humanJointRules(HumanSkeletonConvention::MakeHuman));
    const auto plan=planHumanJointNames(source,HumanSkeletonConvention::CharacterCreatorG3);
    CHECK(plan.changes.size()==51&&plan.skipped.size()==2);CharacterEditor editor(original);CHECK(applyHumanJointNames(editor,source,plan));
    auto &renamed=editor.asset()->object().get();CHECK(renamed.data().bones.size()==163&&renamed.data().meshes.size()==5);
    const auto cc=mapHumanJoints(*editor.asset(),humanJointRules(HumanSkeletonConvention::CharacterCreatorG3));CHECK(cc.completeBody());
    const auto posture=planHumanReferencePose(cc);CHECK(posture.skipped.empty()&&posture.directions.size()==20);
    for(std::size_t i=0;i<original->data().bones.size();++i)
    {CHECK(renamed.data().bones[i].parent==original->data().bones[i].parent);}
    CHECK(renamed.data().semantics.size()==original->data().semantics.size());
    for(std::size_t i=0;i<renamed.data().semantics.size();++i)
        CHECK(renamed.data().semantics[i].name==original->data().semantics[i].name&&renamed.data().semantics[i].bone==original->data().semantics[i].bone);
    std::size_t points=0;for(std::size_t m=0;m<renamed.data().meshes.size();++m)
    {
        const auto &a=*original->data().meshes[m].topology,&b=*renamed.data().meshes[m].topology;
        CHECK(a.points.size()==b.points.size()&&a.layout==b.layout);points+=a.points.size();
        for(std::size_t p=0;p<a.points.size();++p)
        {
            CHECK(a.points[p].position==b.points[p].position&&a.points[p].weights.size()==b.points[p].weights.size());
            for(std::size_t w=0;w<a.points[p].weights.size();++w)
                CHECK(a.points[p].weights[w].bone==b.points[p].weights[w].bone&&std::abs(a.points[p].weights[w].weight-b.points[p].weights[w].weight)<1e-12);
        }
    }
    CHECK(points==43474&&editor.undo()&&editor.asset()->object()==original->object()&&editor.redo());
    const auto fbx=exportObject3DFbx(editor.asset()->object().get(),FbxExportOptions{.allowMorphNormalMetadata=true});
    Object3DEngine imported;CharacterAsset read(importObject3DFbx(imported.reference(),fbx).object);
    const auto roundtrip=mapHumanJoints(read,humanJointRules(HumanSkeletonConvention::CharacterCreatorG3));
    CHECK(roundtrip.completeBody()&&read.data().bones.size()==163&&read.data().meshes.size()==5);
}
int main(int argc,char **argv)
{
    try{if(argc<2)return 2;const std::string_view name=argv[1];if(name=="mapping")mapping();else if(name=="partial")partial();else if(name=="hierarchy")hierarchy();else if(name=="naming")naming();else if(name=="naming_invalid")namingInvalid();else if(name=="naming_asset"&&argc==3)namingAsset(argv[2]);else return 2;std::cout<<name<<" passed\n";return 0;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
