#pragma once
#include <iiActorObject/Animation.h>
#include <iiActorObject/Object3DEngine.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DAnimation.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DProjectAnimation.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#endif
#include <cmath>
#include <iostream>

inline int fbxAnimationConsumer()
{
    using namespace iiActorObject;
    Object3DData data;data.name="InstalledAnimation";data.bones={{"root",NoBone,{}}};
    Object3DMesh mesh;mesh.name="surface";
    mesh.geometry.vertices={{{0,0,0},{0,0,1},{{0,1}}},{{1,0,0},{0,0,1},{{0,1}}},{{0,1,0},{0,0,1},{{0,1}}}};
    mesh.geometry.triangles={0,1,2};mesh.geometry.morphTargets={{"Smile",{{0,{.1,0,0},{}}}}};
    data.meshes.push_back(std::move(mesh));Object3DEngine engine;const auto object=engine.createObject(std::move(data));
    auto end=object.get().skeleton()->bones()[0].bindTransform;end.position.x=1;
    AnimationClip clip(object.get().skeleton(),1,{{0,{{0,{}},{1,end}}}},{{"Smile",{{0,0},{1,1}}}});
    FbxAnimationExportOptions options;options.exportMatchingMorphCurves=true;
    const auto bytes=exportObject3DFbx(object.get(),clip,options);
    if(bytes.size()<100||bytes[0]!='K')return 1;
    Object3DEngine destination;
    const auto imported=importObject3DFbxWithAnimations(destination.reference(),bytes);
    if(imported.animations.size()!=1||imported.animations[0].clip->curves().size()!=1||
       imported.animations[0].morphBindings.size()!=1)return 2;
    Pose pose(imported.asset.object.get().skeleton());imported.animations[0].clip->sample(.5,pose);
    if(std::abs(pose.locals()[0].position.x-.5)>1e-5||
       std::abs(imported.animations[0].clip->sampleCurve(0,.5)-.5)>1e-5)return 3;
    FbxAnimationImportOptions separated;separated.morphGrouping=FbxMorphAnimationGrouping::PerMesh;
    const auto perMesh=importObject3DFbxWithAnimations(destination.reference(),bytes,separated);
    if(perMesh.animations.size()!=1||perMesh.animations[0].morphBindings.size()!=1||
       std::abs(perMesh.animations[0].clip->sampleCurve(perMesh.animations[0].morphBindings[0].curve,.5)-.5)>1e-5)return 5;
    auto instance=perMesh.asset.object.get().instantiate();std::vector<SkeletalMeshComponent*> parts;
    for(auto *component:instance->components())if(auto *part=dynamic_cast<SkeletalMeshComponent*>(component))parts.push_back(part);
    applyObject3DAnimationClip(perMesh.asset.object,*perMesh.animations[0].clip,perMesh.animations[0].morphBindings,.5,instance->pose(),parts);
    if(parts.size()!=1||std::abs(instance->pose().locals()[0].position.x-.5)>1e-5||
       std::abs(parts[0]->morphWeights()[0]-.5)>1e-5)return 6;
    const auto editor=destination.editObject(perMesh.asset.object);const auto &take=perMesh.animations[0];
    const Object3DProjectAnimation animation{take.name,perMesh.asset.object,take.clip,take.morphBindings,take.sourceStartSeconds};
    const auto projectBytes=exportObject3DProjectWithAnimations(editor,std::array{animation});
    Object3DEngine projectEngine;const auto project=importObject3DProject(projectEngine.reference(),projectBytes);
    if(project.animations.size()!=1||project.animations[0].morphBindings.size()!=1||
       std::abs(project.animations[0].clip->sampleCurve(0,.5)-.5)>1e-5||
       exportObject3DProject(project)!=projectBytes)return 7;
    editor.get().moveVertices(0,std::array{VertexMove{0,{0,.01,0}}});
    const auto rebound=rebindObject3DProjectAnimation(animation,editor.get().object());
    if(rebound.object!=editor.get().object()||
       exportObject3DProjectWithAnimations(editor,std::array{rebound}).empty())return 8;
#if IIACTOROBJECT_HAS_NSCHARACTER
    const NSCharacter::CharacterAsset character(object);
    const auto characterBytes=NSCharacter::exportFbx(character,clip,options);
    const auto characterImport=NSCharacter::importFbxWithAnimations(destination.reference(),characterBytes);
    if(!characterImport.asset||!characterImport.asset->valid()||characterImport.animations.size()!=1||
       characterImport.animations[0].clip->skeleton().get()!=characterImport.asset->skeleton().get())return 4;
#endif
    std::cout<<"Installed FBX skeletal and morph animation roundtrip API passed\n";return 0;
}
