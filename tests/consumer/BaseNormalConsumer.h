#pragma once
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DExternalEdits.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DTemplate.h>
#include <array>
#include <cmath>

// The same installed-package consumer runs with and without NSCharacter.
inline int baseNormalConsumer()
{
    using namespace iiActorObject;
    Object3DEngine engine;
    auto source=engine.createObject(parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nf 1//1 2//1 3//1\n"));
    auto identity=makeObject3DExternalIdentity(source.get().data(),"installed-base-normal");
    Object3DExternalOptions options;options.document=identity.document;
    auto data=source.get().data();data.meshes[0].topology->normals[0]={.6,0,.8};auto edited=engine.createObject(data);
    Object3DTemplateDefinition definition;definition.base=source;definition.targets={{"height",0,{{2,{0,.2,0}}}}};
    auto editor=engine.editTemplate(engine.createTemplate(definition));
    const auto report=editor.get().applyExternalEdits(source.get(),{source.get(),identity},{edited.get(),identity},options);
    if(report.normals!=1||!report.layer)return 1;
    const auto bytes=exportObject3DProject(editor);if(bytes.size()<9||bytes[8]!=19)return 2;
    Object3DEngine restored;auto project=importObject3DProject(restored.reference(),bytes);
    project.editor.get().setTemplateValues(std::array{TemplateValue{"height",.5}});
    const auto normal=project.editor.get().object().get().data().meshes[0].topology->normals[0];
    return std::hypot(normal.x-.6,normal.y,normal.z-.8)<1e-8?0:3;
}
