#pragma once
#include <iiActorObject/DualQuaternionSkinning.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DTemplateRigTransfer.h>
#include <iiActorObject/Object3DTemplateAdapt.h>
#include <iiActorObject/SurfaceBinding.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#endif
#include <array>
#include <cmath>
#include <numbers>

inline int dualQuaternionConsumer()
{
    using namespace iiActorObject;
    auto skeleton=std::make_shared<Skeleton>(std::vector<Bone>{{"root",NoBone,{}},{"a",0,{}},{"b",0,{}}});
    SkeletalMeshLod lod;lod.vertices={{{1,0,0},{0,0,1},{{1,.5},{2,.5}}}};
    lod.skinning={SkinningMethod::DualQuaternion,{}};
    auto mesh=std::make_shared<SkeletalMesh>(skeleton,std::vector<SkeletalMeshLod>{lod});auto pose=std::make_shared<Pose>(skeleton);
    pose->setLocal(1,{{},Quaternion::fromAxisAngle({0,0,1},std::numbers::pi/2)});
    pose->setLocal(2,{{},Quaternion::fromAxisAngle({0,0,1},-std::numbers::pi/2)});
    const std::array factors{1.};std::array<DeformedVertex,1> direct;
    deformDualQuaternionSkin(*mesh,*pose,0,{},factors,direct);SkeletalMeshComponent component(mesh,pose);
    const bool authored=std::abs(component.deformedVertices()[0].position.x-1)<1e-9;
    component.setDualQuaternionBlendWeights(std::array{0.});
    const bool overridden=std::abs(component.deformedVertices()[0].position.x)<1e-9;
    component.setDualQuaternionBlendWeights({});
    Object3DEngine engine;auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    auto editor=engine.editObject(engine.createObject(std::move(data)));
    editor.get().setSkinningProfile(0,{SkinningMethod::DualQuaternion,{}});
    Object3DEngine other;const auto restored=importObject3DProject(other.reference(),exportObject3DProject(editor));
    const bool saved=restored.editor.get().object().get().data().meshes[0].topology->skinning.method==SkinningMethod::DualQuaternion;
    auto sourceData=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    sourceData.kind=Object3DKind::SkeletalMesh;sourceData.bones={{"root",NoBone,{}}};
    for(auto &point:sourceData.meshes[0].topology->points)point.weights={{0,1}};
    sourceData.meshes[0].topology->skinning={SkinningMethod::DualQuaternion,{}};
    auto followerData=sourceData;followerData.meshes.push_back(followerData.meshes[0]);
    followerData.meshes[1].name="Follower";followerData.meshes[1].topology->skinning={SkinningMethod::Blended,{.25,.25,.25}};
    const std::array rows{SurfaceControlPoint{0,{0,1,2},{.25,.75,0},{}}};
    const auto binding=bindMeshControlPoints(followerData,0,1,rows);
    (void)transferMeshBindingWeights(followerData,binding,0,NoBone,SkinningProfileTransfer::Source);
    const bool followerProfile=followerData.meshes[1].topology->skinning.blendWeights==std::vector<double>({1,.25,.25});
    const std::array anchors{TemplateJointBinding{0,{0,{0}},{0,{2}}, {}}};
    bindObject3DTemplateJoints(sourceData,anchors);
    Object3DTemplateDefinition definition;definition.base=engine.createObject(sourceData);
    definition.joints.assign(anchors.begin(),anchors.end());
    const auto program=engine.createTemplate(std::move(definition));
    auto targetData=sourceData;targetData.kind=Object3DKind::StaticMesh;targetData.bones.clear();
    targetData.meshes[0].topology->skinning={};
    for(auto &point:targetData.meshes[0].topology->points)point.weights.clear();
    const auto target=engine.createObject(std::move(targetData));TemplateRigTransferOptions options;options.allowUnanchoredRig=true;
    const auto transferred=transferObject3DTemplateRig(program,target,options);
    const auto kept=transferObject3DTemplateRig(program,target,options,SkinningProfileTransfer::PreserveTarget);
    const bool projected=transferred.object.get().data().meshes[0].topology->skinning.method==SkinningMethod::DualQuaternion&&
        kept.object.get().data().meshes[0].topology->skinning.method==SkinningMethod::Linear;
    auto dressed=target.get().data();dressed.bones.clear();dressed.semantics.clear();
    auto garment=dressed.meshes[0];garment.name="Garment";
    for(auto &point:garment.topology->points)point.position.z+=.02;
    dressed.meshes.push_back(std::move(garment));const auto dressedTarget=engine.createObject(std::move(dressed));
    TemplateAdaptOptions adapt;adapt.rig.projection.maximumSurfaceDistance=.05;adapt.rig.allowRigidUnmappedMeshes=true;
    const std::array followers{TemplateAdaptFollower{0,1,true}};adapt.followers=followers;
    const auto adapted=adaptObject3DTemplate(program,dressedTarget,adapt,SkinningProfileTransfer::SourceIfNonlinear,
                                             SkinningProfileTransfer::SourceIfNonlinear);
    const bool adaptedProfile=adapted.program.get().definition().base.get().data().meshes[1].topology->skinning.method==SkinningMethod::DualQuaternion;
#if IIACTOROBJECT_HAS_NSCHARACTER
    NSCharacter::CharacterEditor view(editor);view.setSkinningProfile(0,{SkinningMethod::Linear,{}});
    NSCharacter::CharacterTemplate character(program);const auto wrapped=character.rigMesh(target,options,SkinningProfileTransfer::Source);
    const bool forwarded=editor.get().object().get().data().meshes[0].topology->skinning.method==SkinningMethod::Linear&&
        wrapped.asset->data().meshes[0].topology->skinning.method==SkinningMethod::DualQuaternion&&
        character.adaptMesh(dressedTarget,adapt,SkinningProfileTransfer::SourceIfNonlinear,SkinningProfileTransfer::SourceIfNonlinear)
            .program.get().definition().base.get().data().meshes[1].topology->skinning.method==SkinningMethod::DualQuaternion;
#else
    const bool forwarded=true;
#endif
    return std::abs(direct[0].position.x-1)<1e-9&&authored&&overridden&&saved&&projected&&forwarded&&followerProfile&&adaptedProfile&&
        std::abs(component.deformedVertices()[0].position.x-1)<1e-9?0:1;
}
