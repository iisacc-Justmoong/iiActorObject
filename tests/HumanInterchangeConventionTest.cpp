#include <iiActorObject/NSCharacter/CharacterEditor.h>
#include <iiActorObject/NSCharacter/HumanJointNaming.h>
#include <iiActorObject/NSCharacter/HumanPosePlanner.h>
#include <iiActorObject/NSCharacter/HumanPoseRetarget.h>
#include <iiActorObject/NSCharacter/HumanoidGenerator.h>
#include <iiActorObject/Object3DInterchange.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>
using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
using Expected=std::vector<std::pair<std::string,std::string>>;
Expected blenderNames(bool sanitized)
{
    Expected out{{"Hips","spine"},{"Spine","spine.001"},{"Spine1","spine.002"},
        {"Spine2","spine.003"},{"Neck","spine.004"},{"Neck1","spine.005"},{"Head","spine.006"}};
    for(const auto &[side,suffix]:std::array{std::pair{"Left",".L"},std::pair{"Right",".R"}})
    {
        const auto add=[&](std::string role,std::string source){out.emplace_back(std::string(side)+role,source+suffix);};
        add("Shoulder","shoulder");add("Arm","upper_arm");add("ForeArm","forearm");add("Hand","hand");
        add("UpLeg","thigh");add("Leg","shin");add("Foot","foot");add("ToeBase","toe");
        for(const auto &[role,name]:std::array{std::pair{"Thumb","thumb"},std::pair{"Index","f_index"},
            std::pair{"Middle","f_middle"},std::pair{"Ring","f_ring"},std::pair{"Pinky","f_pinky"}})
            for(unsigned n=1;n<=3;++n)add("Hand"+std::string(role)+std::to_string(n),std::string(name)+".0"+std::to_string(n));
    }
    if(sanitized)for(auto &[role,name]:out)std::replace(name.begin(),name.end(),'.','_');
    return out;
}
Expected g5Names()
{
    Expected out{{"Hips","RL_Hips"},{"Spine","RL_Waist"},{"Spine1","RL_Spine01"},
        {"Neck","RL_Neck"},{"Neck1","RL_NeckTwist01"},{"Head","RL_Head"}};
    for(const auto &[side,code]:std::array{std::pair{"Left","L"},std::pair{"Right","R"}})
    {
        const auto add=[&](std::string role,std::string source){out.emplace_back(std::string(side)+role,"RL_"+std::string(code)+"_"+source);};
        add("Shoulder","Clavicle");add("Arm","UpperArm");add("ForeArm","Forearm");add("Hand","Hand");
        add("UpLeg","Thigh");add("Leg","Calf");add("Foot","Foot");
        add("ArmRoll","UpperArmTwist02");add("ForeArmRoll","ForearmTwist02");
        add("UpLegRoll","ThighTwist02");add("LegRoll","CalfTwist02");
        unsigned finger=0;
        for(const auto role:{"Thumb","Index","Middle","Ring","Pinky"})
        {
            for(unsigned n=0;n<3;++n)add("Hand"+std::string(role)+std::to_string(n+1),"Finger"+std::to_string(finger)+std::to_string(n));
            add("Hand"+std::string(role)+"4","Finger"+std::to_string(finger)+"Nub");++finger;
        }
    }
    return out;
}
struct Fixture final {std::shared_ptr<const CharacterAsset> original;std::shared_ptr<CharacterAsset> target;HumanJointMapping source;};
Fixture fixture(Object3DEngine &engine,const Expected &expected)
{
    auto original=HumanoidGenerator::generate(engine.reference(),{}, {36,false,false});
    auto source=mapHumanJoints(*original,humanJointRules(HumanSkeletonConvention::Procedural));
    auto data=original->data();
    for(const auto &[role,name]:expected)
    {
        auto bone=source.joint(role);
        if(bone==NoBone&&role.ends_with("Roll"))
        {
            const bool left=role.starts_with("Left");const auto suffix=left?"_l":"_r";
            const auto type=role.find("ForeArm")!=role.npos?"lowerarm":role.find("UpLeg")!=role.npos?"thigh":
                role.find("Leg")!=role.npos?"calf":"upperarm";
            bone=original->skeleton()->findBone(std::string(type)+"_twist_02"+suffix);
            if(bone==NoBone)
            {
                const auto parent=source.joint(role.substr(0,role.size()-4));CHECK(parent!=NoBone);
                bone=data.bones.size();data.bones.push_back({"temporary-"+role,parent,{{0,.015,0}}});
            }
        }
        if(bone==NoBone&&role.ends_with("4"))
        {
            auto parentRole=role;parentRole.back()='3';const auto parent=source.joint(parentRole);CHECK(parent!=NoBone);
            bone=data.bones.size();data.bones.push_back({"temporary-"+role,parent,{{.015,0,0}}});
        }
        CHECK(bone!=NoBone);data.bones[bone].name=name;
    }
    if(expected.front().second=="spine")
    {
        // Blender's finger bases are descendants of palm helpers; preserve
        // those intermediate bones instead of assuming direct hand children.
        for(const std::string side:{"Left","Right"})for(const std::string finger:{"Index","Middle","Ring","Pinky"})
        {
            const auto hand=source.joint(side+"Hand"),first=source.joint(side+"Hand"+finger+"1");
            CHECK(hand!=NoBone&&first!=NoBone);const auto palm=data.bones.size();
            data.bones.push_back({"palm-helper-"+side+finger,hand,{}});data.bones[first].parent=palm;
        }
    }
    return {std::move(original),std::make_shared<CharacterAsset>(engine.createObject(std::move(data))),std::move(source)};
}
void mapping(HumanSkeletonConvention convention,const Expected &expected,bool shouldPlanFeet)
{
    Object3DEngine engine;const auto f=fixture(engine,expected);const auto map=mapHumanJoints(*f.target,humanJointRules(convention));
    CHECK(map.completeBody()&&map.bindings().size()==expected.size());
    for(const auto &[role,name]:expected){const auto bone=map.joint(role);CHECK(bone!=NoBone&&f.target->data().bones[bone].name==name);}
    const auto plan=planHumanReferencePose(map);
    CHECK(shouldPlanFeet?plan.skipped.empty():plan.skipped.size()==2);
    CharacterEditor editor(f.target);setHumanOrderedReferencePose(editor,map,plan.directions,plan.frames,plan.order,plan.referenceOptions);
    CHECK(editor.asset()->data().referencePose&&engine.reference().owns(editor.asset()->object()));
    const auto retarget=planHumanPoseRetarget(map,f.source);CHECK(!retarget.joints.empty());
    const auto profile=exportCc5BoneMap(map);const std::string text(profile.begin(),profile.end());
    CHECK(text.find(expected.front().second+" = Hips")!=text.npos);
    for(bool fbx:{false,true})
    {
        Object3DEngine imported;
        const auto object=fbx?importObject3DFbx(imported.reference(),exportObject3DFbx(f.target->object().get())).object:
                              importObject3DGlb(imported.reference(),exportObject3DGlb(f.target->object().get())).object;
        const auto roundtrip=mapHumanJoints(CharacterAsset(object),humanJointRules(convention));
        CHECK(roundtrip.completeBody()&&roundtrip.bindings().size()==expected.size());
        CHECK(object.get().data().meshes.size()==f.target->data().meshes.size());
    }
}
void invalid()
{
    Object3DEngine engine;const auto f=fixture(engine,blenderNames(false));auto data=f.target->data();
    const auto index=data.bones.size();const auto arm=f.target->skeleton()->findBone("upper_arm.L");CHECK(arm!=NoBone);
    data.bones[arm].parent=0;auto bad=std::make_shared<CharacterAsset>(engine.createObject(std::move(data)));
    const auto count=engine.objectCount();rejects([&]{(void)mapHumanJoints(*bad,humanJointRules(HumanSkeletonConvention::BlenderMetaRig));});
    CHECK(engine.objectCount()==count&&index==bad->data().bones.size());
}
void naming()
{
    Object3DEngine engine;const auto f=fixture(engine,blenderNames(false));
    const auto source=mapHumanJoints(*f.target,humanJointRules(HumanSkeletonConvention::BlenderMetaRig));
    const auto names=planHumanJointNames(source,HumanSkeletonConvention::CharacterCreatorG5);
    CharacterEditor editor(f.target);CHECK(applyHumanJointNames(editor,source,names));
    const auto converted=mapHumanJoints(*editor.asset(),humanJointRules(HumanSkeletonConvention::CharacterCreatorG5));
    CHECK(converted.completeBody()&&converted.joint("LeftArm")==source.joint("LeftArm"));
    CHECK(editor.undo()&&editor.asset()->object()==f.target->object()&&editor.redo());
}
void automatic()
{
    Object3DEngine engine;
    for(const auto &[convention,expected]:std::array{
        std::pair{HumanSkeletonConvention::CharacterCreatorG5,g5Names()},
        std::pair{HumanSkeletonConvention::BlenderMetaRig,blenderNames(false)},
        std::pair{HumanSkeletonConvention::BlenderMetaRigSanitized,blenderNames(true)}})
    {
        const auto f=fixture(engine,expected);
        for(bool exchange:{false,true})
        {
            Object3DEngine destination;
            const auto object=exchange?importObject3DFbx(destination.reference(),exportObject3DFbx(f.target->object().get())).object:
                                       importObject3DGlb(destination.reference(),exportObject3DGlb(f.target->object().get())).object;
            const auto result=detectHumanSkeletonConvention(CharacterAsset(object));
            CHECK(result.status==HumanConventionDetectionStatus::Unique);
            CHECK(result.completeConventions.size()==1&&result.completeConventions[0]==convention);
            CHECK(result.mapping&&result.mapping->completeBody());
            for(const auto &[role,name]:expected)
                CHECK(object.get().data().bones[result.mapping->joint(role)].name==name);
            const auto plan=planHumanJointNames(*result.mapping,HumanSkeletonConvention::CharacterCreatorG3);
            CHECK(!plan.changes.empty());
            CharacterEditor editor(std::make_shared<CharacterAsset>(object));
            CHECK(applyHumanJointNames(editor,*result.mapping,plan));
            const auto converted=detectHumanSkeletonConvention(*editor.asset());
            CHECK(converted.status==HumanConventionDetectionStatus::Unique&&converted.mapping);
            CHECK(converted.completeConventions==std::vector{HumanSkeletonConvention::CharacterCreatorG3});
            CHECK(editor.undo()&&editor.asset()->object()==object&&editor.redo());
        }
    }
    auto procedural=HumanoidGenerator::generate(engine.reference(),{}, {36,false,false});
    const auto shared=detectHumanSkeletonConvention(*procedural);
    CHECK(shared.status==HumanConventionDetectionStatus::Equivalent);
    CHECK(shared.mapping&&shared.mapping->completeBody()&&shared.completeConventions.size()==2);
    CHECK(std::find(shared.completeConventions.begin(),shared.completeConventions.end(),HumanSkeletonConvention::Unreal)!=shared.completeConventions.end());
    CHECK(std::find(shared.completeConventions.begin(),shared.completeConventions.end(),HumanSkeletonConvention::Procedural)!=shared.completeConventions.end());
    const std::array selection{HumanSkeletonConvention::Unreal};
    const auto narrowed=detectHumanSkeletonConvention(*procedural,selection);
    CHECK(narrowed.status==HumanConventionDetectionStatus::Unique&&narrowed.mapping);
}
void ambiguous()
{
    Object3DEngine engine;const auto f=fixture(engine,g5Names());auto data=f.target->data();
    const auto proceduralRules=humanJointRules(HumanSkeletonConvention::Procedural);
    std::set<std::string> recipeNames;for(const auto &rule:proceduralRules)recipeNames.insert(rule.source);
    for(auto &bone:data.bones)if(recipeNames.contains(bone.name))bone.name="first-helper:"+bone.name;
    const auto firstCount=data.bones.size();
    for(auto bone:f.original->data().bones)
    {
        if(!recipeNames.contains(bone.name))bone.name="second-helper:"+bone.name;
        if(bone.parent!=NoBone)bone.parent+=firstCount;else bone.parent=0;
        data.bones.push_back(std::move(bone));
    }
    auto combined=std::make_shared<CharacterAsset>(engine.createObject(std::move(data)));
    const auto result=detectHumanSkeletonConvention(*combined);
    CHECK(result.status==HumanConventionDetectionStatus::Ambiguous&&!result.mapping);
    CHECK(std::find(result.completeConventions.begin(),result.completeConventions.end(),HumanSkeletonConvention::CharacterCreatorG5)!=result.completeConventions.end());
    CHECK(std::find(result.completeConventions.begin(),result.completeConventions.end(),HumanSkeletonConvention::Procedural)!=result.completeConventions.end());
    const std::array selection{HumanSkeletonConvention::CharacterCreatorG5};
    const auto resolved=detectHumanSkeletonConvention(*combined,selection);
    CHECK(resolved.status==HumanConventionDetectionStatus::Unique&&resolved.mapping);
}
void detectionFailures()
{
    Object3DEngine engine;const auto f=fixture(engine,g5Names());auto data=f.target->data();
    const auto rightHand=f.target->skeleton()->findBone("RL_R_Hand");CHECK(rightHand!=NoBone);
    data.bones[rightHand].name="missing-right-hand";
    CharacterAsset partial(engine.createObject(data));
    const auto incomplete=detectHumanSkeletonConvention(partial);
    CHECK(incomplete.status==HumanConventionDetectionStatus::NoMatch&&!incomplete.mapping);
    const auto partialCandidate=std::find_if(incomplete.candidates.begin(),incomplete.candidates.end(),[](const auto &c)
        {return c.convention==HumanSkeletonConvention::CharacterCreatorG5;});
    CHECK(partialCandidate!=incomplete.candidates.end()&&partialCandidate->mapping);
    CHECK(std::find(partialCandidate->mapping->missingRequired().begin(),partialCandidate->mapping->missingRequired().end(),"RightHand")!=partialCandidate->mapping->missingRequired().end());
    data=f.target->data();
    const auto arm=f.target->skeleton()->findBone("RL_L_UpperArm");CHECK(arm!=NoBone);data.bones[arm].parent=0;
    CharacterAsset invalid(engine.createObject(std::move(data)));
    const auto bad=detectHumanSkeletonConvention(invalid);
    CHECK(bad.status==HumanConventionDetectionStatus::NoMatch&&!bad.mapping);
    const auto candidate=std::find_if(bad.candidates.begin(),bad.candidates.end(),[](const auto &c)
        {return c.convention==HumanSkeletonConvention::CharacterCreatorG5;});
    CHECK(candidate!=bad.candidates.end()&&!candidate->mapping&&!candidate->rejectionReason.empty());
    const std::array duplicate{HumanSkeletonConvention::CharacterCreatorG5,HumanSkeletonConvention::CharacterCreatorG5};
    rejects([&]{(void)detectHumanSkeletonConvention(*f.target,duplicate);});
    const std::array unknown{static_cast<HumanSkeletonConvention>(999)};
    rejects([&]{(void)detectHumanSkeletonConvention(*f.target,unknown);});
    HumanJointMappingOptions limited;limited.maximumRules=1;
    bool budgetRejected=false;
    try{(void)detectHumanSkeletonConvention(*f.target,{},limited);}
    catch(const std::length_error&){budgetRejected=true;}
    CHECK(budgetRejected);
    limited.maximumRules=256;limited.maximumAncestorVisits=0;budgetRejected=false;
    try{(void)detectHumanSkeletonConvention(*f.target,{},limited);}
    catch(const std::length_error&){budgetRejected=true;}
    CHECK(budgetRejected);
    auto procedural=HumanoidGenerator::generate(engine.reference(),{}, {36,false,false});
    const auto source=mapHumanJoints(*procedural,humanJointRules(HumanSkeletonConvention::Procedural));
    auto tagged=procedural->data();
    for(const auto &rule:humanJointRules(HumanSkeletonConvention::CharacterCreatorG5))
    {
        const auto bone=source.joint(rule.role);
        if(bone!=NoBone)tagged.semantics.push_back({rule.source,bone});
    }
    const auto result=detectHumanSkeletonConvention(CharacterAsset(engine.createObject(std::move(tagged))));
    CHECK(result.status==HumanConventionDetectionStatus::Equivalent);
    CHECK(std::find(result.completeConventions.begin(),result.completeConventions.end(),HumanSkeletonConvention::CharacterCreatorG5)==result.completeConventions.end());
}
void detectionLifetime()
{
    HumanConventionDetection result;
    {
        Object3DEngine engine;const auto f=fixture(engine,g5Names());
        result=detectHumanSkeletonConvention(*f.target);CHECK(result.mapping&&result.mapping->valid());
    }
    CHECK(result.mapping&&!result.mapping->valid());
    rejects([&]{(void)exportCc5BoneMap(*result.mapping);});
}
void detectAsset(const char *path)
{
    std::ifstream input(path,std::ios::binary);CHECK(input.good());
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};
    Object3DEngine engine;const auto imported=importObject3DFbx(engine.reference(),bytes);
    const auto result=detectHumanSkeletonConvention(CharacterAsset(imported.object));
    CHECK(result.status==HumanConventionDetectionStatus::Unique&&result.mapping&&result.mapping->completeBody());
    CHECK(result.completeConventions==std::vector{HumanSkeletonConvention::MakeHuman});
    const auto plan=planHumanReferencePose(*result.mapping);CHECK(!plan.directions.empty());
}
int main(int argc,char **argv)
{
    try
    {
        if(argc<2)return 2;const std::string_view mode=argv[1];
        if(mode=="profile_g5"||mode=="profile_blender")
        {
            Object3DEngine engine;const bool g5=mode=="profile_g5";
            const auto f=fixture(engine,g5?g5Names():blenderNames(true));
            const auto map=mapHumanJoints(*f.target,humanJointRules(g5?HumanSkeletonConvention::CharacterCreatorG5:HumanSkeletonConvention::BlenderMetaRigSanitized));
            const auto bytes=exportCc5BoneMap(map);std::cout.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));return 0;
        }
        if(mode=="g5")mapping(HumanSkeletonConvention::CharacterCreatorG5,g5Names(),false);
        else if(mode=="blender")mapping(HumanSkeletonConvention::BlenderMetaRig,blenderNames(false),true);
        else if(mode=="blender_sanitized")mapping(HumanSkeletonConvention::BlenderMetaRigSanitized,blenderNames(true),true);
        else if(mode=="invalid")invalid();else if(mode=="naming")naming();
        else if(mode=="automatic")automatic();else if(mode=="ambiguous")ambiguous();
        else if(mode=="detection_failures")detectionFailures();else if(mode=="detection_lifetime")detectionLifetime();
        else if(mode=="detect_asset"&&argc==3)detectAsset(argv[2]);else return 2;
        std::cout<<mode<<" passed\n";return 0;
    }
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
