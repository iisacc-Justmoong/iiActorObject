#pragma once
#include <iiActorObject/Features.h>
#include <iiActorObject/Object3DProgressiveMorph.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DProject.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#endif
#include <array>
#include <cmath>
#include <fstream>
#include <iterator>

inline int progressiveMorphConsumer(const char *path,const char *animationPath)
{
    using namespace iiActorObject;
    std::ifstream stream(path,std::ios::binary);if(!stream)return 1;
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream),{}};
    Object3DEngine engine;const auto imported=importObject3DFbxWithProgressiveMorphs(engine.reference(),bytes);
    if(!imported.rig.valid()||!imported.asset.object.valid())return 2;
    const auto info=inspectObject3DProgressiveMorphRig(imported.rig);
    if(info.channels.size()!=1||info.channels[0].keys.size()!=5||info.channels[0].sourceName!="Expression")return 3;
    auto actor=imported.asset.object.get().instantiate();auto *mesh=actor->findComponent<SkeletalMeshComponent>();
    if(!mesh||std::abs(mesh->deformedVertices()[0].position.y-.125)>1e-10)return 4;
    imported.rig.get().applyTo(*actor,imported.rig.get().evaluateNamed(std::array{NamedRigControl{"Expression",.55}}));
    if(std::abs(mesh->deformedVertices()[0].position.y-.525)>1e-10)return 5;
    const auto editor=engine.editObject(imported.asset.object);const auto project=exportObject3DProject(editor,std::array{imported.rig});
    if(project.size()<24||project[8]!=31)return 6;
    Object3DEngine restoredEngine;const auto restored=importObject3DProject(restoredEngine.reference(),project);
    if(restored.rigs.size()!=1||inspectObject3DProgressiveMorphRig(restored.rigs[0])!=info)return 7;
    const auto fbx=exportObject3DFbxWithProgressiveMorphs(restored.rigs[0]);
    if(fbx.bytes.empty()||fbx.warnings.empty())return 9;
    Object3DEngine roundEngine;const auto round=importObject3DFbxWithProgressiveMorphs(roundEngine.reference(),fbx.bytes);
    if(!round.rig.valid()||inspectObject3DProgressiveMorphRig(round.rig).channels[0].keys.size()!=5)return 10;
    auto instances=imported.asset.object.get().data();auto other=instances.meshes[0];other.name="Face2";
    instances.meshes.push_back(std::move(other));auto sharedInfo=info;sharedInfo.channels[0].sourceMeshes.push_back("Face2");
    const auto sharedRig=createObject3DProgressiveMorphRig(engine.createObject(std::move(instances)),sharedInfo);
    const auto sharedFbx=exportObject3DFbxWithProgressiveMorphs(sharedRig);
    Object3DEngine sharedEngine;const auto shared=importObject3DFbxWithProgressiveMorphs(sharedEngine.reference(),sharedFbx.bytes);
    if(!shared.rig.valid()||inspectObject3DProgressiveMorphRig(shared.rig).channels[0].sourceMeshes.size()!=2)return 12;
    std::ifstream animatedStream(animationPath,std::ios::binary);if(!animatedStream)return 13;
    const std::vector<std::uint8_t> animatedBytes{std::istreambuf_iterator<char>(animatedStream),{}};
    Object3DEngine animatedEngine;
    const auto animated=importObject3DFbxWithProgressiveMorphsAndAnimations(animatedEngine.reference(),animatedBytes);
    if(!animated.rig.valid()||animated.animations.size()!=1)return 14;
    auto animatedActor=animated.asset.object.get().instantiate();std::vector<SkeletalMeshComponent *> parts;
    for(auto *component:animatedActor->components())if(auto *part=dynamic_cast<SkeletalMeshComponent *>(component))parts.push_back(part);
    if(parts.size()!=1)return 15;
    applyObject3DProgressiveAnimationClip(animated.rig,*animated.animations[0].clip,animated.animations[0].morphBindings,1,animatedActor->pose(),parts);
    if(std::abs(parts[0]->deformedVertices()[0].position.y-.8041666666666667)>2e-4)return 16;
    FbxAnimationExportOptions animationOptions;animationOptions.indexedBindings=animated.animations[0].morphBindings;
    const auto animatedFbx=exportObject3DFbxWithProgressiveMorphs(animated.rig,*animated.animations[0].clip,animationOptions);
    Object3DEngine animatedRoundEngine;const auto animatedRound=importObject3DFbxWithProgressiveMorphsAndAnimations(animatedRoundEngine.reference(),animatedFbx.bytes);
    if(!animatedRound.rig.valid()||animatedRound.animations.size()!=1)return 17;
    const auto takesFbx=exportObject3DFbxWithProgressiveMorphs(animated.rig,std::span(animated.animations));
    Object3DEngine takesEngine;const auto takesRound=importObject3DFbxWithProgressiveMorphsAndAnimations(takesEngine.reference(),takesFbx.bytes);
    if(takesRound.animations.size()!=1||std::abs(takesRound.animations[0].sourceStartSeconds+1)>1e-10)return 19;
    const auto &take=animated.animations[0];
    std::vector<Object3DProjectAnimation> projectTakes{{take.name,animated.asset.object,take.clip,take.morphBindings,take.sourceStartSeconds}};
    if(exportObject3DFbxWithProgressiveMorphs(animated.rig,std::span(projectTakes)).bytes.empty())return 20;
#if IIACTOROBJECT_HAS_NSCHARACTER
    Object3DEngine humanEngine;const auto human=NSCharacter::importFbxWithProgressiveMorphs(humanEngine,bytes);
    if(!human.asset||!human.rig.valid()||human.rig.get().object()!=human.asset->object())return 8;
    if(NSCharacter::exportFbxWithProgressiveMorphs(*human.asset,human.rig).bytes.empty())return 11;
    const auto humanAnimated=NSCharacter::importFbxWithProgressiveMorphsAndAnimations(humanEngine,animatedBytes);
    if(!humanAnimated.asset||!humanAnimated.rig.valid()||humanAnimated.animations.size()!=1)return 18;
    if(NSCharacter::exportFbxWithProgressiveMorphs(*humanAnimated.asset,humanAnimated.rig,std::span(humanAnimated.animations)).bytes.empty())return 21;
#endif
    return 0;
}
