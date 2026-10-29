#pragma once
#include <iiActorObject/Features.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/PoseReference.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#include <iiActorObject/NSCharacter/HumanPoseDirections.h>
#include <iiActorObject/NSCharacter/HumanoidGenerator.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <tuple>

inline int groupPoseConsumer()
{
    using namespace iiActorObject;
    Object3DEngine engine;auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    data.bones={{"root",NoBone,{}},{"joint",0,{{0,1,0}}},
                {"toe-a",1,{{.1,.2,0}}},{"toe-b",1,{{-.1,.2,0}}}};
    const auto source=engine.createObject(std::move(data));const auto editor=engine.editObject(source);
    PoseReference reference;reference.directions={{1,2,{0,0,1},{3}}};editor.get().setReferencePose(reference);
    const auto bytes=exportObject3DProject(editor);if(bytes.size()<24||bytes[8]!=30)return 1;
    const auto loaded=importObject3DProject(engine.reference(),bytes);
    if(loaded.editor.get().object().get().data().referencePose!=reference)return 2;
    auto actor=loaded.editor.get().object().get().instantiate();const auto world=actor->pose().worldMatrices();
    const auto origin=world[1].transformPoint({}),a=world[2].transformPoint({}),b=world[3].transformPoint({});
    const Vector3 delta{(a.x+b.x)*.5-origin.x,(a.y+b.y)*.5-origin.y,(a.z+b.z)*.5-origin.z};
    if(std::abs(delta.x)>1e-10||std::abs(delta.y)>1e-10||delta.z<=0)return 3;
    if(!loaded.editor.get().undo()||!loaded.editor.get().redo())return 4;
#if IIACTOROBJECT_HAS_NSCHARACTER
    using namespace iiActorObject::NSCharacter;
    CharacterAsset character(source);const std::array rules{HumanJointRule{"LeftFoot","joint",HumanJointSource::BoneName}};
    const auto mapping=mapHumanJoints(character,rules);CharacterEditor human(engine.editObject(source));
    const std::array directions{HumanJointDirection{"LeftFoot","",{0,0,1},{2,3}}};
    setHumanOrderedReferencePose(human,mapping,directions,{},{});
    if(human.asset()->data().referencePose!=reference)return 5;
    for(const auto &[convention,role,name]:std::array{
        std::tuple{HumanSkeletonConvention::CharacterCreatorG5,"Hips","RL_Hips"},
        std::tuple{HumanSkeletonConvention::BlenderMetaRig,"LeftArm","upper_arm.L"},
        std::tuple{HumanSkeletonConvention::BlenderMetaRigSanitized,"LeftArm","upper_arm_L"}})
    {
        const auto ruleset=humanJointRules(convention);
        if(std::none_of(ruleset.begin(),ruleset.end(),[&](const auto &r){return r.role==role&&r.source==name;}))return 6;
    }
    const auto generated=HumanoidGenerator::generate(engine.reference(),{}, {36,false,false});
    const auto detection=detectHumanSkeletonConvention(*generated);
    if(detection.status!=HumanConventionDetectionStatus::Equivalent||!detection.mapping||
       !detection.mapping->completeBody()||detection.completeConventions.size()!=2)return 7;
    const std::array onlyUnreal{HumanSkeletonConvention::Unreal};
    const auto selected=detectHumanSkeletonConvention(*generated,onlyUnreal);
    if(selected.status!=HumanConventionDetectionStatus::Unique||!selected.mapping||
       selected.mapping->joint("Hips")==NoBone)return 8;
#endif
    return 0;
}
