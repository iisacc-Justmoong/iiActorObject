#pragma once
#include <iiActorObject/Features.h>
#include <iiActorObject/Object3DPoseCorrectives.h>
#include <iiActorObject/Object3DPoseCorrectiveAnimation.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#endif
#include <array>
#include <cmath>

inline int poseCorrectiveConsumer()
{
    using namespace iiActorObject;
    Object3DEngine engine;auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    data.kind=Object3DKind::SkeletalMesh;data.bones={{"root",NoBone,{}},{"elbow",0,{{0,1,0}}}};
    for(auto &point:data.meshes[0].topology->points)point.weights={{1,1}};
    data.meshes[0].topology->morphTargets={{"fold",{{0,{0,0,.2}}}}};
    const auto object=engine.createObject(std::move(data));const auto peak=Quaternion::fromAxisAngle({1,0,0},1);
    Object3DPoseCorrectiveProgram program{{{"fold",{{"elbow",peak,1}},1}}};
    const auto rig=createObject3DPoseCorrectiveRig(object,program);auto actor=object.get().instantiate();
    auto &component=attachObject3DPoseCorrectives(*actor,rig);actor->beginPlay();
    auto local=actor->pose().locals()[1];local.rotation=peak;actor->pose().setLocal(1,local);actor->tick(0);
    const auto *mesh=actor->findComponent<SkeletalMeshComponent>();if(!mesh||component.error()||std::abs(mesh->morphWeights()[0]-1)>1e-9)return 1;
    const auto editor=engine.editObject(object);const auto project=exportObject3DProject(editor,std::array{rig});
    if(project.size()<25||project[8]!=26)return 2;
    Object3DEngine restored;const auto loaded=importObject3DProject(restored.reference(),project);
    if(loaded.rigs.size()!=1)return 3;auto next=loaded.rigs[0].get().object().get().instantiate();
    local=next->pose().locals()[1];local.rotation=peak;next->pose().setLocal(1,local);
    applyObject3DPoseCorrectives(loaded.rigs[0],*next);
    if(std::abs(next->findComponent<SkeletalMeshComponent>()->morphWeights()[0]-1)>1e-9)return 4;
    auto start=object.get().skeleton()->bones()[1].bindTransform,end=start;end.rotation=peak;
    const AnimationClip motion(object.get().skeleton(),1,{{1,{{0,start},{1,end}}}});
    const auto baked=bakeObject3DPoseCorrectiveAnimation(rig,motion);
    if(!baked.maximumConservativeWeightError||baked.clip.curves().size()!=1||baked.clip.curves()[0].name!="fold"||
       std::abs(baked.clip.sampleCurve(0,.5)-.5)>1e-6)return 6;
#if IIACTOROBJECT_HAS_NSCHARACTER
    Object3DTemplateDefinition definition;definition.base=object;
    NSCharacter::CharacterTemplate character(engine.createTemplate(std::move(definition)));
    NSCharacter::CharacterAsset view(object);const auto wrapped=character.createPoseCorrectiveRig(view,program);
    if(!wrapped.valid())return 5;
#endif
    return 0;
}
