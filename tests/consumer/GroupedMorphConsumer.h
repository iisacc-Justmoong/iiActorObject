#pragma once
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <algorithm>

// Runs against the staged package in both feature and core-only configurations.
inline int groupedMorphConsumer()
{
    using namespace iiActorObject;
    auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    data.kind=Object3DKind::SkeletalMesh;data.bones={{"root",NoBone,{}}};
    for(auto &p:data.meshes[0].topology->points)p.weights={{0,1}};
    data.meshes[0].topology->morphTargets={{"Smile",{{1,{0,0,.1}}},10},{"Smile",{{1,{0,0,.2}}},11}};
    Object3DEngine engine;auto source=engine.createObject(std::move(data));
    FbxExportOptions output;output.externalDocument="installed-grouped-morph";
    FbxImportOptions input;input.restoreAuthoringMetadata=false;
    auto baseline=importObject3DFbx(engine.reference(),exportObject3DFbx(source.get(),output),input);
    auto editedData=baseline.object.get().data();auto &target=editedData.meshes[0].topology->morphTargets.at(0);
    if(editedData.meshes[0].topology->morphTargets.size()!=1)return 1;
    const auto point=std::find_if(target.deltas.begin(),target.deltas.end(),[](const auto &v){return v.point==1;});
    if(point==target.deltas.end())return 2;point->position.z+=.05;target.name="Grin";
    auto edited=engine.createObject(std::move(editedData));auto editor=engine.editObject(source);
    Object3DExternalOptions options;options.document=output.externalDocument;
    if(editor.get().applyExternalEdits(source.get(),{baseline.object.get(),baseline.externalIdentity},
        {edited.get(),baseline.externalIdentity},options).morphs!=1)return 3;
    const auto &records=editor.get().object().get().data().meshes[0].topology->morphTargets;
    if(records.size()!=2||records[0].sourceTarget!=10||records[1].sourceTarget!=11||records[1].name!="Grin")return 4;
    Object3DEngine restored;auto project=importObject3DProject(restored.reference(),exportObject3DProject(editor));
    return project.editor.get().object().get().data().meshes[0].topology->morphTargets[1].name=="Grin"?0:5;
}
