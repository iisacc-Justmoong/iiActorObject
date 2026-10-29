#pragma once
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DTemplateRigTransfer.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/MeshSurface.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#endif
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

// Uses only installed headers and works with and without NSCharacter.
inline int templateRigYawConsumer()
{
    using namespace iiActorObject;
    const std::array vertices{Vector3{0,0,0},Vector3{1,0,0},Vector3{0,1,0}};
    const std::array<std::uint32_t,6> faces{0,2,1,0,1,2};MeshSurface surface(vertices,faces);
    SurfaceQueryBudget budget;budget.maximumNodeVisits=100;budget.maximumTriangleTests=100;
    const auto compatible=surface.closestCompatible({.25,.25,.1},{0,0,1},.9,1,budget);
    if(!compatible.valid()||compatible.triangle!=1)return 3;
    auto source=parseObject3DObj("v 0 0 0\nv 2 0 0\nv 0 2 0\nv 0 0 1\n"
        "v .3 .2 .02\nv .3 .8 .02\nf 1 3 2\nf 1 2 4\nf 2 3 4\nf 3 1 4\n");
    source.kind=Object3DKind::SkeletalMesh;source.bones={{"root",NoBone,{}}};
    for(auto &point:source.meshes[0].topology->points)point.weights={{0,1}};
    source.meshes[0].topology->skinning={SkinningMethod::DualQuaternion,{}};
    const std::array joints{TemplateJointBinding{0,{0,{4}},{0,{5}},{}}};
    bindObject3DTemplateJoints(source,joints);Object3DEngine engine;
    Object3DTemplateDefinition definition;definition.base=engine.createObject(source);
    definition.joints.assign(joints.begin(),joints.end());const auto program=engine.createTemplate(std::move(definition));
    auto target=source;target.kind=Object3DKind::StaticMesh;target.bones.clear();target.meshes[0].topology->skinning={};
    constexpr double angle=.5;const auto c=std::cos(angle),s=std::sin(angle);
    for(auto &point:target.meshes[0].topology->points)
    {
        const auto p=point.position;point.position={c*p.x+s*p.z+1,p.y,-s*p.x+c*p.z+2};point.weights.clear();
    }
    for(auto &vertex:target.meshes[0].geometry.vertices)vertex.weights.clear();
    const auto input=engine.createObject(std::move(target));TemplateRigTransferOptions options;
    options.skin=TemplateRigSkin::ClosestSurface;options.anchors=TemplateRigAnchorMethod::ProjectSurface;
    options.projection.registration=TemplateRigRegistration::YawAndBounds;options.projection.maximumSurfaceDistance=.005;
    const auto result=transferObject3DTemplateRig(program,input,options);
    if(!result.object.valid()||!engine.reference().owns(result.object)||result.report.anchoredJoints!=1)return 1;
    const auto &profile=result.object.get().data().meshes[0].topology->skinning;
    if(profile.method!=SkinningMethod::Blended||profile.blendWeights.size()!=6)return 4;
    for(std::size_t i=0;i<profile.blendWeights.size();++i)
        if(std::abs(profile.blendWeights[i]-(i<4?1.:0.))>1e-9)return 5;
    auto independentTarget=source;SkinTransferOptions skinOptions;skinOptions.includeUnusedPoints=false;
    SkinNormalTransferOptions normalOptions;SurfaceQueryBudget skinBudget;
    const auto normalTransfer=SkinBinder::transferWithNormalTies(source,0,independentTarget,0,skinOptions,
        SkinningProfileTransfer::PreserveTarget,normalOptions,skinBudget);
    if(normalTransfer.transfer.transferred!=4||skinBudget.nodeVisits==0)return 10;
    const auto sourceHandle=engine.createObject(source);auto blank=source;
    for(auto &point:blank.meshes[0].topology->points)point.weights.clear();
    const auto editing=engine.editObject(engine.createObject(std::move(blank)));skinBudget={};
    const auto edited=editing.get().transferWeightsWithNormalTies(0,sourceHandle.get(),0,skinOptions,
        SkinningProfileTransfer::PreserveTarget,normalOptions,skinBudget);
    if(edited.transfer.transferred!=4||!editing.get().undo()||!editing.get().redo())return 11;
#if IIACTOROBJECT_HAS_NSCHARACTER
    if(!editing.get().undo())return 12;
    const NSCharacter::CharacterAsset sourceView(sourceHandle);NSCharacter::CharacterEditor character(editing);skinBudget={};
    const auto humanEdit=character.transferWeightsWithNormalTies(0,sourceView,0,skinOptions,
        SkinningProfileTransfer::PreserveTarget,normalOptions,skinBudget);
    if(humanEdit.transfer.transferred!=4||!character.undo()||!character.redo())return 13;
#endif
    auto pair=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    auto follower=pair.meshes[0];follower.name="Follower";pair.meshes.push_back(std::move(follower));
    std::array<std::optional<SurfaceBindingAxisScale>,3> spans{};spans[0]=SurfaceBindingAxisScale{0,1,1};
    const std::array rows{SurfaceControlPoint{0,{0,0,0},{1,0,0},{.1,0,0}}};
    auto binding=bindMeshControlPoints(pair,0,1,rows,spans);
    const SurfaceBindingBasis basis{Vector3{2,0,0},Vector3{0,1,0},Vector3{0,0,1}};
    if(std::abs(evaluateMeshSurfaceBindingWithBasis(pair,binding,basis)[0].x-.1)>1e-9)return 6;
    auto turned=pair;
    for(auto &mesh:turned.meshes)for(auto &point:mesh.topology->points)
    {const auto p=point.position;point.position={p.z,p.y,p.x};}
    const SurfaceBindingBasis turnedBasis{Vector3{0,0,2},Vector3{0,1,0},Vector3{1,0,0}};
    const auto turnedBinding=bindMeshControlPointsWithBasis(turned,0,1,rows,spans,turnedBasis);
    if(std::abs(evaluateMeshSurfaceBindingWithBasis(turned,turnedBinding,turnedBasis)[0].z-.1)>1e-9)return 9;
    Object3DTemplateDefinition saved;saved.base=engine.createObject(pair);saved.surfaces={binding};saved.surfaceOffsetBases={basis};
    const auto editor=engine.editTemplate(engine.createTemplate(std::move(saved)));
    const auto bytes=exportObject3DProject(editor);if(bytes[8]!=25)return 7;
    Object3DEngine destination;const auto restored=importObject3DProject(destination.reference(),bytes);
    const auto &restoredBinding=restored.editor.get().templateProgram().get().definition().surfaces.at(0);
    const auto &bases=restored.editor.get().templateProgram().get().definition().surfaceOffsetBases;
    if(bases.size()!=1||!bases[0]||std::abs(evaluateMeshSurfaceBindingWithBasis(
        restored.editor.get().object().get().data(),restoredBinding,*bases[0])[0].x-.1)>1e-9)return 8;
    return std::abs(std::remainder(result.report.projection.referenceYawRadians-angle,2*std::numbers::pi))<.001?0:2;
}
