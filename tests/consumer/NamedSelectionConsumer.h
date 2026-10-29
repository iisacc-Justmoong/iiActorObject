#pragma once
#include <array>
#include <cmath>
#include <iiActorObject/Features.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#endif

inline int namedSelectionConsumer()
{
    using namespace iiActorObject;
    Object3DEngine engine;Object3DTemplateDefinition definition;
    definition.base=engine.createObject(parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));
    auto editor=engine.editTemplate(engine.createTemplate(std::move(definition)));
    const MeshPointMask mask{0,{1,.5,0}};editor.get().setPointSelection("Face",std::span(&mask,1));
    editor.get().moveControlPoints(0,std::array{TopologyPointMove{0,{0,0,.1}},TopologyPointMove{1,{0,0,.1}}});
    editor.get().setLayerPointSelection(editor.get().layers()[0].id,"Face");
    const auto &points=editor.get().object().get().data().meshes[0].topology->points;
    if(std::abs(points[0].position.z-.1)>1e-10||std::abs(points[1].position.z-.05)>1e-10)return 1;
    const auto bytes=exportObject3DProject(editor);if(bytes.size()<9||bytes[8]!=17)return 2;
    Object3DEngine target;auto loaded=importObject3DProject(target.reference(),bytes);
    if(loaded.editor.get().pointSelection("Face")[0]!=mask||!loaded.editor.get().undo()||!loaded.editor.get().redo())return 3;
#if IIACTOROBJECT_HAS_NSCHARACTER
    iiActorObject::NSCharacter::CharacterEditor view(loaded.editor);
    if(view.pointSelection("Face")[0]!=mask||&view.session().get()!=&loaded.editor.get())return 4;
#endif
    return 0;
}
