#pragma once

#include <array>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>

// Compiled against staged installed headers in both feature and core-only packages.
inline int multiUvConsumer()
{
    using namespace iiActorObject;
    auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                               "vt 0 0\nvt 1 0\nvt 0 1\nf 1/1 2/2 3/3\n");
    auto &topology=*data.meshes[0].topology;
    topology.primaryUvName="Surface";
    TopologyTextureCoordinateSet extra;
    extra.name="Detail";
    extra.coordinates={{0,0},{.5,0},{0,.5}};
    for(const auto &layout:topology.layout->definition().layouts)
        extra.layoutIndices.push_back(layout.textureCoordinate);
    topology.additionalUvSets.push_back(std::move(extra));

    Object3DEngine engine;
    auto editor=engine.editObject(engine.createObject(std::move(data)));
    editor.get().setTextureCoordinates(0,1,std::array{TopologyUvEdit{1,{.7,.2}}});
    if(!editor.get().undo() || !editor.get().redo())return 1;
    const auto project=exportObject3DProject(editor);
    if(project.size()<9 || project[8]!=16)return 2;
    Object3DEngine restoredOwner;
    const auto restored=importObject3DProject(restoredOwner.reference(),project);
    const auto &mesh=restored.editor.get().object().get().data().meshes[0];
    if(mesh.topology->additionalUvSets[0].coordinates[1]!=TextureCoordinate{.7,.2})return 3;
    const auto fbx=importObject3DFbx(restoredOwner.reference(),exportObject3DFbx(restored.editor.get().object().get()));
    const auto &decoded=*fbx.object.get().data().meshes[0].topology;
    if(decoded.primaryUvName!="Surface" || decoded.additionalUvSets[0].name!="Detail" ||
       decoded.additionalUvSets[0].coordinates[1]!=TextureCoordinate{.7,.2})return 4;
    return exportObject3DGlb(fbx.object.get()).size()>100 ? 0 : 5;
}
