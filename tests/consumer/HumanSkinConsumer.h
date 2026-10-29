#pragma once
#include <iiActorObject/NSCharacter/HumanSkin.h>
#include <iiActorObject/Object3DObj.h>
#include <iostream>

inline int humanSkinConsumer(const char *directory)
{
    using namespace iiActorObject;
    using namespace iiActorObject::NSCharacter;
    const auto presets=listHumanSkinPresets(directory);if(presets.size()!=20)return 1;
    Object3DEngine engine;
    auto source=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nf 1/1 2/2 3/3\n");
    auto view=std::make_shared<CharacterAsset>(engine.createObject(std::move(source)));CharacterEditor editor(view);
    HumanSkinSelection skin;skin.directory=directory;skin.preset=presets.front().id;
    if(!setHumanSkin(editor,skin))return 2;
    const auto &data=editor.asset()->data();const auto index=data.materials[data.meshes[0].material].baseColorImage;
    if(!index||data.images[*index].width()!=2048||data.images[*index].height()!=2048)return 3;
    if(setHumanSkin(editor,skin)||!editor.undo()||!editor.redo())return 4;
    std::cout<<"Installed human skin selection and history passed\n";return 0;
}
