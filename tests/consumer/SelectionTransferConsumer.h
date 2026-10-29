#pragma once
#include <iiActorObject/Features.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#endif
#include <array>
#include <cmath>

inline int selectionTransferConsumer()
{
    using namespace iiActorObject;
    Object3DEngine engine;const auto source=engine.createObject(parseObject3DObj(
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));
    const auto target=engine.createObject(parseObject3DObj(
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nv .25 .25 0\nf 1 2 4\nf 2 3 4\nf 3 1 4\n"));
    const auto editor=engine.editObject(source);const MeshPointMask mask{0,{1,0,0}};
    editor.get().setPointSelection("Face",std::span(&mask,1));
    const std::array pairs{MeshMaskProjectionPair{0,0}};
#if IIACTOROBJECT_HAS_NSCHARACTER
    NSCharacter::CharacterEditor character(editor);
    const auto report=character.replaceWithProjectedSelections(target,pairs);
#else
    const auto report=editor.get().replaceWithProjectedSelections(target,pairs);
#endif
    if(report.selectionSets!=1||report.projectedPoints!=4||editor.get().object()!=target||
       std::abs(editor.get().pointSelection("Face")[0].weights[3]-.5)>1e-8)return 1;
    const auto bytes=exportObject3DProject(editor);if(bytes[8]!=28)return 2;
    Object3DEngine restored;const auto project=importObject3DProject(restored.reference(),bytes);
    if(!project.editor.get().undo()||project.editor.get().pointSelection("Face")[0]!=mask||
       !project.editor.get().redo())return 3;
    return 0;
}
