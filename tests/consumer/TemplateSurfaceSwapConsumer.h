#pragma once
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <array>

inline int templateSurfaceSwapConsumer()
{
    using namespace iiActorObject;
    Object3DEngine engine;
    const auto make=[&](bool alternative)
    {
        auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
        data.bones={{"root",NoBone,{}}};for(auto &point:data.meshes[0].topology->points)point.weights={{0,1}};
        auto follower=data.meshes[0];follower.name=alternative?"New":"Old";follower.material=1;data.meshes.push_back(follower);
        Object3DMaterial material;material.name=alternative?"NewMaterial":"OldMaterial";data.materials.push_back(material);
        const std::array rows{SurfaceControlPoint{0,{0,0,0},{1,0,0},{0,0,.1}},SurfaceControlPoint{1,{1,1,1},{1,0,0},{0,0,.1}},SurfaceControlPoint{2,{2,2,2},{1,0,0},{0,0,.1}}};
        const auto binding=bindMeshControlPoints(data,0,1,rows);fitMeshSurfaceBinding(data,binding);(void)transferMeshBindingWeights(data,binding);
        Object3DTemplateDefinition definition;definition.base=engine.createObject(std::move(data));definition.surfaces={binding};
        return engine.createTemplate(std::move(definition));
    };
    const auto original=make(false),replacement=make(true);auto editor=engine.editTemplate(original);
    editor.get().moveControlPoints(0,std::array{TopologyPointMove{0,{0,0,.02}}});
    if(!editor.get().replaceTemplateSurfaces(replacement,std::array<std::size_t,1>{1}))return 1;
    if(editor.get().object().get().data().meshes[1].name!="New")return 2;
    if(!editor.get().undo()||editor.get().object().get().data().meshes[1].name!="Old"||!editor.get().redo())return 3;
    auto copy=editor.get().object().get().data();TemplateGrounding grounding;
    grounding.supportPoints={0};grounding.offset=2;
    (void)groundObject3D(copy,grounding);
    if(copy.meshes[0].topology->points[0].position.y!=2)return 4;
    const auto makeRemoval=[&](bool withFirst)
    {
        auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
        data.bones={{"root",NoBone,{}}};for(auto &point:data.meshes[0].topology->points)point.weights={{0,1}};
        std::vector<SurfaceBinding> bindings;
        for(const auto *name:{"First","Second"})
        {
            if(!withFirst&&std::string_view(name)=="First")continue;
            auto follower=data.meshes[0];follower.name=name;data.meshes.push_back(std::move(follower));
            const auto target=data.meshes.size()-1;
            const std::array rows{SurfaceControlPoint{0,{0,0,0},{1,0,0},{0,0,.1}},SurfaceControlPoint{1,{1,1,1},{1,0,0},{0,0,.1}},SurfaceControlPoint{2,{2,2,2},{1,0,0},{0,0,.1}}};
            bindings.push_back(bindMeshControlPoints(data,0,target,rows));fitMeshSurfaceBinding(data,bindings.back());(void)transferMeshBindingWeights(data,bindings.back());
        }
        Object3DTemplateDefinition definition;definition.base=engine.createObject(std::move(data));definition.surfaces=std::move(bindings);
        return engine.createTemplate(std::move(definition));
    };
    const auto full=makeRemoval(true),fewer=makeRemoval(false);auto removing=engine.editTemplate(full);
    removing.get().setPointSelection("Second",std::array{MeshPointMask{2,{1,0,0}}});
    if(!removing.get().removeTemplateSurfaces(fewer,std::array<std::size_t,1>{1})||removing.get().pointSelection("Second")[0].mesh!=1)return 5;
    const auto archive=exportObject3DProject(removing);if(archive[8]!=33)return 6;
    Object3DEngine destination;auto project=importObject3DProject(destination.reference(),archive);
    if(!project.editor.get().undo()||project.editor.get().pointSelection("Second")[0].mesh!=2||
       !project.editor.get().redo()||project.editor.get().pointSelection("Second")[0].mesh!=1)return 7;
    auto adding=engine.editTemplate(fewer);adding.get().setPointSelection("Second",std::array{MeshPointMask{1,{1,0,0}}});
    if(!adding.get().addTemplateSurfaces(full,std::array<std::size_t,1>{1})||adding.get().pointSelection("Second")[0].mesh!=2)return 8;
    const auto inserted=exportObject3DProject(adding);if(inserted[8]!=34)return 9;
    Object3DEngine insertionDestination;auto insertedProject=importObject3DProject(insertionDestination.reference(),inserted);
    if(!insertedProject.editor.get().undo()||insertedProject.editor.get().pointSelection("Second")[0].mesh!=1||
       !insertedProject.editor.get().redo()||insertedProject.editor.get().pointSelection("Second")[0].mesh!=2)return 10;
    auto mixedDefinition=fewer.get().definition();auto mixedData=mixedDefinition.base.get().data();
    auto third=mixedData.meshes[1];third.name="Third";mixedData.meshes.push_back(std::move(third));
    const std::array rows{SurfaceControlPoint{0,{0,0,0},{1,0,0},{0,0,.1}},SurfaceControlPoint{1,{1,1,1},{1,0,0},{0,0,.1}},SurfaceControlPoint{2,{2,2,2},{1,0,0},{0,0,.1}}};
    const auto binding=bindMeshControlPoints(mixedData,0,2,rows);fitMeshSurfaceBinding(mixedData,binding);(void)transferMeshBindingWeights(mixedData,binding);
    mixedDefinition.base=engine.createObject(std::move(mixedData));mixedDefinition.surfaces.push_back(binding);
    const auto alternative=engine.createTemplate(std::move(mixedDefinition));auto mixed=engine.editTemplate(full);
    mixed.get().setPointSelection("Second",std::array{MeshPointMask{2,{1,0,0}}});
    if(!mixed.get().changeTemplateSurfaces(alternative,std::array<std::size_t,1>{1},std::array<std::size_t,1>{2})||
       mixed.get().pointSelection("Second")[0].mesh!=1)return 11;
    const auto mixedBytes=exportObject3DProject(mixed);if(mixedBytes[8]!=35)return 12;
    Object3DEngine mixedDestination;auto mixedProject=importObject3DProject(mixedDestination.reference(),mixedBytes);
    if(!mixedProject.editor.get().undo()||mixedProject.editor.get().pointSelection("Second")[0].mesh!=2||
       !mixedProject.editor.get().redo()||mixedProject.editor.get().pointSelection("Second")[0].mesh!=1)return 13;
    return 0;
}
