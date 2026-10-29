#pragma once
#include <iiActorObject/Object3DInstanceTransfer.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DPoseRig.h>
#include <iiActorObject/Object3DProject.h>
#include <array>
#include <cmath>

inline int instanceTransferConsumer()
{
    using namespace iiActorObject;
    Object3DEngine engine;auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    data.bones={{"root",NoBone,{}},{"jaw",0,{{0,1,0}}}};data.semantics={{"Root",0},{"Jaw",1}};
    for(auto &point:data.meshes[0].topology->points)point.weights={{1,1}};
    data.meshes[0].topology->morphTargets.push_back({"Smile",{{0,{0,0,.1}}}});
    const auto source=engine.createObject(std::move(data));auto actor=source.get().instantiate();
    auto first=source.get().data().bones[1].bindTransform,last=first;last.position.y=1.5;
    auto clip=std::make_shared<AnimationClip>(source.get().skeleton(),2,std::vector<BoneTrack>{{1,{{0,first},{2,last}}}});
    actor->animation().play(clip,AnimationWrap::Clamp);actor->animation().seek(.5);actor->animation().pause();
    actor->findComponent<SkeletalMeshComponent>()->setMorphWeight(0,.6);
    auto editor=engine.editObject(source);Object3DPoseProgram program;program.joints={"root","jaw"};
    program.units={{"SmileRig",{{},Quaternion::fromAxisAngle({0,0,1},.3)}}};
    const auto rig=createObject3DPoseRig(source,program);Object3DEditorRigSet rigs(editor,std::array{rig});
    editor.get().moveJoint(1,{0,2,0});
    auto copied=transferObject3DEditorInstance(source,*actor,rigs);
    if(!copied.report.copiedAnimation||!copied.report.rigsRebound||copied.actor->animation().wrap()!=AnimationWrap::Clamp||
       std::abs(copied.actor->animation().time()-.5)>1e-10||copied.actor->animation().isPlaying()||
       std::abs(copied.actor->findComponent<SkeletalMeshComponent>()->morphWeights()[0]-.6)>1e-10)return 1;
    const auto evaluated=rigs.evaluateNamed(0,std::array{NamedRigControl{"SmileRig",1}});
    rigs.rig(0).get().applyTo(*copied.actor,evaluated);
    if(copied.actor->pose().skeleton()!=editor.get().object().get().skeleton()||
       std::abs(copied.actor->pose().locals()[1].position.y-2)>1e-10)return 2;
    const Object3DProjectAnimation savedAnimation{"Take",editor.get().object(),copied.actor->animation().clip(),{},{}};
    const auto bytes=exportObject3DProjectWithAnimations(editor,std::array{savedAnimation},rigs.rigs());
    Object3DEngine destination;const auto restored=importObject3DProject(destination.reference(),bytes);
    if(restored.rigs.size()!=1||restored.animations.size()!=1||restored.animations[0].clip->skeleton()!=restored.editor.get().object().get().skeleton())return 3;
    return 0;
}
