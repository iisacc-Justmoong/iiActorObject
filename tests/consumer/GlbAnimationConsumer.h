#pragma once
#include <iiActorObject/GlbAnimationPose.h>
#include <iiActorObject/GlbAnimationBake.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#endif
#include <cmath>
#include <iostream>

inline int glbAnimationConsumer()
{
    using namespace iiActorObject;
    Object3DData data;data.name="InstalledGlbMotion";data.bones={{"root",NoBone,{}}};
    Object3DMesh mesh;mesh.name="Face";
    mesh.geometry.vertices={{{0,0,0},{0,0,1},{{0,1}}},{{1,0,0},{0,0,1},{{0,1}}},{{0,1,0},{0,0,1},{{0,1}}}};
    mesh.geometry.triangles={0,1,2};mesh.geometry.morphTargets={{"Smile",{{0,{.1,0,0},{}}}}};data.meshes.push_back(std::move(mesh));
    Object3DEngine owner;const auto original=owner.createObject(std::move(data));
    auto end=original.get().skeleton()->bones()[0].bindTransform;end.position.x=1;
    AnimationClip clip(original.get().skeleton(),1,{{0,{{0,{}},{1,end}}}},{{"Smile",{{0,0},{1,1}}}});
    GlbMorphAnimationOptions morph;morph.matchUnmappedCurvesByName=true;
    const auto bytes=exportObject3DGlb(original.get(),clip,{},morph);
    const auto imported=importObject3DGlbWithAnimations(owner.reference(),bytes);
    if(imported.animations.size()!=1)return 1;
    const auto sample=sampleGlbAnimationPose(imported.asset.object,imported.nodes,imported.animations[0],.5);
    if(sample.boneLocals.size()!=1||sample.morphs.size()!=1||std::abs(sample.boneLocals[0].position.x-.5)>1e-5||
       std::abs(sample.morphs[0].value-.5)>1e-5)return 2;
    auto instance=imported.asset.object.get().instantiate();std::vector<SkeletalMeshComponent*> components;
    for(auto *part:instance->components())if(auto *item=dynamic_cast<SkeletalMeshComponent*>(part))components.push_back(item);
    applyGlbAnimationPose(sample,instance->pose(),components);
    if(components.size()!=1||std::abs(instance->pose().locals()[0].position.x-.5)>1e-5||
       std::abs(components[0]->morphWeights()[0]-.5)>1e-5)return 3;
    const auto baked=bakeGlbAnimationClip(imported.asset.object,imported.nodes,imported.animations[0]);
    const auto sampled=sampleGlbBakedAnimation(baked,.5);
    if(sampled.morphs.size()!=1||baked.morphBindings.size()!=1||
       std::abs(sampled.boneLocals[0].position.x-.5)>1e-5||std::abs(sampled.morphs[0].value-.5)>1e-5)return 5;
    GlbMorphAnimationOptions indexed;indexed.indexedBindings=baked.morphBindings;
    if(exportObject3DGlb(imported.asset.object.get(),baked.clip,{},indexed).empty())return 6;
    FbxAnimationExportOptions fbx;fbx.indexedBindings=baked.morphBindings;
    if(exportObject3DFbx(imported.asset.object.get(),baked.clip,fbx).empty())return 7;
#if IIACTOROBJECT_HAS_NSCHARACTER
    const auto character=NSCharacter::importGlbWithAnimations(owner,bytes);
    if(!character.asset||!character.asset->valid()||character.animations.size()!=1)return 4;
#endif
    std::cout<<"Installed GLB animation import, pose and morph sampling passed\n";return 0;
}
