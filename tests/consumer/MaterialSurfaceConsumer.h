#pragma once
#include <iiActorObject/Features.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#endif

inline int materialSurfaceConsumer()
{
    using namespace iiActorObject;
    Object3DEngine engine;
    auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    data.materials[0].baseColor[3]=.4;
    const auto source=engine.createObject(std::move(data));
    const auto editor=engine.editObject(source);
    const Object3DMaterialSurface hair{Object3DAlphaMode::Mask,.25,true};
    if(!editor.get().setMaterialSurface(0,hair)||editor.get().setMaterialSurface(0,hair))return 1;
    if(effectiveAlphaMode(editor.get().object().get().data().materials[0])!=Object3DAlphaMode::Mask)return 2;
    const auto project=exportObject3DProject(editor);
    if(project.size()<24||project[8]!=29)return 3;
    const auto restored=importObject3DProject(engine.reference(),project);
    if(restored.editor.get().object().get().data().materials[0].surface!=hair)return 4;
    if(!restored.editor.get().undo()||!restored.editor.get().redo())return 5;
    const auto glb=importObject3DGlb(engine.reference(),exportObject3DGlb(editor.get().object().get()));
    if(glb.object.get().data().materials[0].surface!=hair)return 6;
    const auto fbx=importObject3DFbx(engine.reference(),exportObject3DFbx(editor.get().object().get()));
    if(fbx.object.get().data().materials[0].surface!=hair)return 7;
#if IIACTOROBJECT_HAS_NSCHARACTER
    NSCharacter::CharacterEditor character(engine.editObject(source));
    if(!character.setMaterialSurface(0,hair)||character.asset()->data().materials[0].surface!=hair)return 8;
#endif
    return 0;
}
