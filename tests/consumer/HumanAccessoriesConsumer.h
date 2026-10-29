#pragma once
#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#include <algorithm>
#include <array>

inline int humanAccessoriesConsumer(const char *body,const char *assets)
{
    using namespace iiActorObject;
    using namespace iiActorObject::NSCharacter;
    const auto entries=listMakeHumanAccessories(assets);
    if(entries.size()!=6||std::none_of(entries.begin(),entries.end(),[](const auto &entry){return entry.id=="hair/short01";}))return 1;
    Object3DEngine engine;MakeHumanTemplateOptions options;
    options.accessories=HumanAccessorySelection{assets,{"hair/short01"},true};
    const auto human=loadMakeHumanTemplate(engine.reference(),body,options);
    auto editor=human.edit();
    if(!engine.reference().owns(editor.asset()->object())||editor.asset()->data().meshes.size()!=2)return 2;
    editor.setTemplateValues(std::array{TemplateValue{"model/head/head-scale-horiz-decr-incr",.5}});
    if(!editor.undo()||!editor.redo())return 3;
    options.accessories->componentIds={"hair/bob01"};const auto alternative=loadMakeHumanTemplate(engine.reference(),body,options);
    if(!setHumanAccessories(editor,alternative,assets)||editor.asset()->data().meshes[1].name!="HairBob01")return 4;
    if(!editor.undo()||editor.asset()->data().meshes[1].name!="HairShort01"||!editor.redo())return 5;
    options.accessories.reset();const auto bare=loadMakeHumanTemplate(engine.reference(),body,options);
    if(!setHumanAccessories(editor,bare,assets)||editor.asset()->data().meshes.size()!=1||
       !editor.undo()||editor.asset()->data().meshes[1].name!="HairBob01")return 6;
    if(!editor.redo()||!setHumanAccessories(editor,alternative,assets)||editor.asset()->data().meshes[1].name!="HairBob01"||
       !editor.undo()||editor.asset()->data().meshes.size()!=1||!editor.redo())return 7;
    options.accessories=HumanAccessorySelection{assets,{"eyebrows/eyebrow001"},true};
    const auto brows=loadMakeHumanTemplate(engine.reference(),body,options);
    if(!setHumanAccessories(editor,brows,assets)||editor.asset()->data().meshes[1].name!="Eyebrows001"||
       !editor.undo()||editor.asset()->data().meshes[1].name!="HairBob01"||!editor.redo())return 8;
    return 0;
}
