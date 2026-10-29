#pragma once
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <fstream>
#include <iterator>

// Compiled against the staged public C++23 package, including the core-only build.
inline int normalMapConsumer(const char *filename)
{
    using namespace iiActorObject;
    std::ifstream image(filename,std::ios::binary);
    if(!image)return 1;
    auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nf 1/1 2/2 3/3\n");
    data.images.emplace_back("Normals",std::vector<std::uint8_t>((std::istreambuf_iterator<char>(image)),{}));
    data.materials[0].normalImage=0;data.materials[0].normalUvSet="UVMap";data.materials[0].normalScale=.4;
    Object3DEngine engine;const auto editor=engine.editObject(engine.createObject(std::move(data)));
    const auto bytes=exportObject3DProject(editor);if(bytes.size()<9||bytes[8]!=18)return 2;
    Object3DEngine target;const auto loaded=importObject3DProject(target.reference(),bytes);
    const auto &m=loaded.editor.get().object().get().data().materials[0];
    if(!m.normalImage||m.normalUvSet!="UVMap"||m.normalScale!=.4)return 3;
    const auto fbx=importObject3DFbx(target.reference(),exportObject3DFbx(loaded.editor.get().object().get()));
    if(!fbx.object.get().data().materials[0].normalImage)return 4;
    return exportObject3DGlb(fbx.object.get()).size()>100?0:5;
}
