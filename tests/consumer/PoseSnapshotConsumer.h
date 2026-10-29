#pragma once
#include <iiActorObject/Features.h>
#include <iiActorObject/Animation.h>
#include <iiActorObject/Object3DPoseSnapshot.h>
#include <iiActorObject/Object3DPoseMorphs.h>
#include <iiActorObject/Object3DRigSnapshot.h>
#include <iiActorObject/Object3DTemplateMorphs.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/PoseRetarget.h>
#include <iiActorObject/AnimationRetarget.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/GlbDocument.h>
#include <array>
#include <cmath>
#include <iostream>
#include <utility>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#include <iiActorObject/NSCharacter/HumanPoseDirections.h>
#include <iiActorObject/NSCharacter/HumanPosePlanner.h>
#include <iiActorObject/NSCharacter/HumanPoseRetarget.h>
#include <iiActorObject/NSCharacter/HumanJointNaming.h>
#include <iiActorObject/NSCharacter/HumanoidGenerator.h>
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#endif

inline int poseSnapshotConsumer()
{
    using namespace iiActorObject;Object3DHandle held,deformedHeld;
    {
        Object3DEngine engine;Object3DData data;data.bones={{"root",NoBone,{}},{"tip",2,{{.5,0,0}}},{"arm",0,{{0,.6,0}}}};
        Object3DMesh mesh;mesh.name="InstalledPose";
        mesh.geometry.vertices={{{.2,.6,0},{0,0,1},{{1,.4},{2,.6}}},{{.8,.6,0},{0,0,1},{{1,.4},{2,.6}}},{{.2,.9,0},{0,0,1},{{1,.4},{2,.6}}}};
        mesh.geometry.triangles={0,1,2};mesh.geometry.morphTargets={{"Shape",{{0,{0,0,.1},{.2,0,0}}}}};data.meshes.push_back(std::move(mesh));
        const auto source=engine.createObject(data);const std::array aims{PoseJointAim{2,1,{0,1,0}}};held=createAimedObject3DPoseSnapshot(source,aims);
        if(engine.objectCount()!=2||!engine.reference().owns(held)||held.id()==source.id()||held.get().data().bones[1].parent!=2)return 110;
        Pose expected(source.get().skeleton());expected.setLocal(2,aimPoseJoint(expected,2,1,{0,1,0}));Pose rest(held.get().skeleton());
        std::array<DeformedVertex,3> before,after;source.get().meshes()[0]->deform(expected,0,std::array{.5},before);held.get().meshes()[0]->deform(rest,0,std::array{.5},after);
        for(std::size_t i=0;i<before.size();++i)
        {
            if(std::hypot(before[i].position.x-after[i].position.x,before[i].position.y-after[i].position.y,before[i].position.z-after[i].position.z)>1e-10)return 111;
            if(std::hypot(before[i].normal.x-after[i].normal.x,before[i].normal.y-after[i].normal.y,before[i].normal.z-after[i].normal.z)>1e-10)return 112;
        }
        const std::vector<std::vector<double>> baseline{{.35}};
        deformedHeld=createObject3DDeformationSnapshot(source,expected.locals(),baseline);
        if(!engine.reference().owns(deformedHeld)||deformedHeld.id()==source.id())return 126;
        Pose deformedRest(deformedHeld.get().skeleton());source.get().meshes()[0]->deform(expected,0,std::array{.85},before);
        deformedHeld.get().meshes()[0]->deform(deformedRest,0,std::array{.5},after);
        for(std::size_t i=0;i<before.size();++i)
        {
            if(std::hypot(before[i].position.x-after[i].position.x,before[i].position.y-after[i].position.y,before[i].position.z-after[i].position.z)>1e-10)return 127;
            if(std::hypot(before[i].normal.x-after[i].normal.x,before[i].normal.y-after[i].normal.y,before[i].normal.z-after[i].normal.z)>1e-10)return 128;
        }
        auto dqData=data;dqData.meshes[0].geometry.skinning={SkinningMethod::Blended,{1,.5,.25}};
        const auto dqSource=engine.createObject(std::move(dqData));Pose dqPose(dqSource.get().skeleton());
        auto dqLocal=dqPose.locals()[2];dqLocal.rotation=Quaternion::fromAxisAngle({0,0,1},.6);dqPose.setLocal(2,dqLocal);
        const auto dqSnapshot=createObject3DDeformationSnapshot(dqSource,dqPose.locals(),baseline);
        if(!engine.reference().owns(dqSnapshot)||dqSnapshot.get().data().meshes[0].geometry.skinning.method!=SkinningMethod::Blended)return 144;
        Pose dqRest(dqSnapshot.get().skeleton());dqSource.get().meshes()[0]->deform(dqPose,0,std::array{.85},before);
        dqSnapshot.get().meshes()[0]->deform(dqRest,0,std::array{.5},after);
        for(std::size_t i=0;i<before.size();++i)
        {
            if(std::hypot(before[i].position.x-after[i].position.x,before[i].position.y-after[i].position.y,before[i].position.z-after[i].position.z)>1e-10)return 145;
            if(std::hypot(before[i].normal.x-after[i].normal.x,before[i].normal.y-after[i].normal.y,before[i].normal.z-after[i].normal.z)>1e-10)return 146;
        }
        const auto linearDq=createObject3DDeformationSnapshot(dqSource,dqPose.locals(),baseline,{},Object3DSnapshotSkinning::LinearizeAtBind);
        if(!engine.reference().owns(linearDq)||linearDq.get().data().meshes[0].geometry.skinning.method!=SkinningMethod::Linear)return 147;
        Pose linearRest(linearDq.get().skeleton());linearDq.get().meshes()[0]->deform(linearRest,0,std::array{.5},after);
        for(std::size_t i=0;i<before.size();++i)
        {
            if(std::hypot(before[i].position.x-after[i].position.x,before[i].position.y-after[i].position.y,before[i].position.z-after[i].position.z)>1e-10)return 148;
            if(std::hypot(before[i].normal.x-after[i].normal.x,before[i].normal.y-after[i].normal.y,before[i].normal.z-after[i].normal.z)>1e-10)return 149;
        }
        const auto glb=exportObject3DGlb(linearDq.get());if(glb.size()<20||glb[0]!='g'||glb[1]!='l'||glb[2]!='T'||glb[3]!='F')return 150;
        GlbImportOptions importOptions;importOptions.maximumSkins=1;importOptions.maximumSkinJointMappings=3;
        const auto restoredGlb=importObject3DGlb(engine.reference(),glb,importOptions);
        if(!engine.reference().owns(restoredGlb.object)||restoredGlb.object.get().data().meshes[0].geometry.morphTargets.size()!=1||
           restoredGlb.skinMappings.size()!=1||restoredGlb.skinMappings[0].sourceSkinIndex!=0||
           restoredGlb.skinMappings[0].sourceJointToBone!=restoredGlb.sourceJointToBone)return 156;
        auto tangentData=data;tangentData.meshes[0].textureCoordinates={{0,0},{1,0},{0,1}};
        tangentData.images.emplace_back("Normal",std::vector<std::uint8_t>{
            0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,
            0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x08,0x08,0x06,0x00,0x00,0x00,0xc4,0x0f,0xbe,0x8b,
            0x00,0x00,0x00,0x11,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0x60,0x60,0xf8,0xff,0x1f,0x3f,
            0x1e,0x11,0x0a,0x00,0x43,0x57,0x7f,0x81,0x9b,0x5b,0x75,0x3b,0x00,0x00,0x00,0x00,
            0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82},"image/png");
        tangentData.materials[0].normalImage=0;
        const auto tangentObject=engine.createObject(std::move(tangentData));
        const auto importedTangents=importObject3DGlb(engine.reference(),exportObject3DGlb(tangentObject.get()));
        const auto &tangentTopology=*importedTangents.object.get().data().meshes[0].topology;
        if(!tangentTopology.sourceGlbTangents||!topologyGlbTangentsValid(tangentTopology)||
           tangentTopology.sourceGlbTangents->tangents.empty()||tangentTopology.sourceGlbTangents->morphTangents.size()!=1)return 158;
        const auto tangentEditor=engine.editObject(importedTangents.object);const auto tangentProject=exportObject3DProject(tangentEditor);
        if(tangentProject.size()<24||tangentProject[8]<24)
        {
            std::cerr<<"Unexpected tangent project version: "<<(tangentProject.size()>8?int(tangentProject[8]):-1)<<'\n';
            return 159;
        }
        Object3DEngine tangentRestoredEngine;
        const auto tangentRestored=importObject3DProject(tangentRestoredEngine.reference(),tangentProject);
        const auto &restoredTangentTopology=*tangentRestored.editor.get().object().get().data().meshes[0].topology;
        if(!restoredTangentTopology.sourceGlbTangents||!topologyGlbTangentsValid(restoredTangentTopology))return 160;
        const auto &originalTangents=*tangentTopology.sourceGlbTangents;
        const auto &restoredTangents=*restoredTangentTopology.sourceGlbTangents;
        if(originalTangents.tangents.size()!=restoredTangents.tangents.size()||
           originalTangents.morphTangents.size()!=restoredTangents.morphTangents.size())return 161;
        for(std::size_t i=0;i<originalTangents.tangents.size();++i)
        {
            const auto &a=originalTangents.tangents[i],&b=restoredTangents.tangents[i];
            if(a.layout!=b.layout||a.direction!=b.direction||a.handedness!=b.handedness)return 162;
        }
        for(std::size_t i=0;i<originalTangents.morphTangents.size();++i)
        {
            const auto &a=originalTangents.morphTangents[i],&b=restoredTangents.morphTangents[i];
            if(a.size()!=b.size())return 163;
            for(std::size_t j=0;j<a.size();++j)
                if(a[j].layout!=b[j].layout||a[j].direction!=b[j].direction)return 164;
        }
        const AnimationClip morphClip(linearDq.get().skeleton(),1,{},{{"Shape",{{0,0},{1,1}}}});
        GlbMorphAnimationOptions morphExport;morphExport.matchUnmappedCurvesByName=true;
        const auto morphAnimatedGlb=exportObject3DGlb(linearDq.get(),morphClip,{},morphExport);
        if(morphAnimatedGlb.size()<=glb.size()||morphAnimatedGlb[0]!='g'||morphAnimatedGlb[1]!='l'||morphAnimatedGlb[2]!='T'||morphAnimatedGlb[3]!='F')return 153;
#if IIACTOROBJECT_HAS_NSCHARACTER
        const NSCharacter::CharacterAsset characterView(linearDq);
        if(NSCharacter::exportGlb(characterView,morphClip,{},morphExport)!=morphAnimatedGlb)return 154;
        const auto restoredCharacter=NSCharacter::importGlb(engine.reference(),glb);
        if(!restoredCharacter.asset||!engine.reference().owns(restoredCharacter.asset->object())||
           restoredCharacter.skinMappings.size()!=1||restoredCharacter.skinMappings[0].sourceJointToBone!=restoredCharacter.sourceJointToBone)return 157;
#endif
        const auto morphRig=createObject3DMorphChannelRig(dqSource);
        const auto morphEvaluation=morphRig.get().evaluateNamed(std::array{NamedRigControl{"Shape",.6}});
        auto morphActor=dqSource.get().instantiate();morphRig.get().applyTo(*morphActor,morphEvaluation);
        auto *morphPart=morphActor->findComponent<SkeletalMeshComponent>();
        if(!morphPart||morphPart->morphWeights().size()!=1||std::abs(morphPart->morphWeights()[0]-.6)>1e-12)return 152;
        const std::array frame{PoseJointFrame{0,1,2,{1,0,0},{0,0,1}}};const auto oriented=createOrientedObject3DPoseSnapshot(source,{},frame);
        if(!engine.reference().owns(oriented)||oriented.id()==source.id())return 115;
        Pose orientedPose(oriented.get().skeleton());const auto worlds=orientedPose.worldMatrices();const auto origin=worlds[0].transformPoint({}),tip=worlds[1].transformPoint({});
        if(tip.x<=origin.x||std::abs(tip.y-origin.y)>1e-10||std::abs(tip.z-origin.z)>1e-10)return 116;
        PoseReference reference;reference.directions.assign(aims.begin(),aims.end());data.referencePose=reference;const auto authoring=engine.createObject(data);
        const auto preview=authoring.get().instantiate();if(authoring.get().referencePoseLocals().size()!=3)return 118;
        const auto current=preview->pose().worldMatrices();const auto delta=Vector3{current[1].transformPoint({}).x-current[2].transformPoint({}).x,current[1].transformPoint({}).y-current[2].transformPoint({}).y,current[1].transformPoint({}).z-current[2].transformPoint({}).z};
        if(std::abs(delta.x)>1e-10||delta.y<=0||std::abs(delta.z)>1e-10)return 119;
        auto edits=engine.editObject(authoring);edits.get().setReferencePose(std::nullopt);if(!edits.get().undo())return 120;
        const auto project=exportObject3DProject(edits);if(project.size()<24||project[8]<13)return 121;auto read=importObject3DProject(engine.reference(),project);
        if(read.editor.get().object().get().data().referencePose!=data.referencePose)return 122;
        const auto referenceSnapshot=createReferenceObject3DPoseSnapshot(authoring);if(referenceSnapshot.get().data().referencePose)return 123;
        PoseReference ordered;ordered.directions.assign(aims.begin(),aims.end());ordered.frames.assign(frame.begin(),frame.end());
        ordered.order={{PoseReferenceStepKind::Frame,0},{PoseReferenceStepKind::Direction,0}};
        data.referencePose=ordered;const auto orderedAuthoring=engine.createObject(data);auto orderedEditor=engine.editObject(orderedAuthoring);
        const auto orderedProject=exportObject3DProject(orderedEditor);if(orderedProject.size()<24||orderedProject[8]<15)return 130;
        const auto orderedRestored=importObject3DProject(engine.reference(),orderedProject);
        if(orderedRestored.editor.get().object().get().data().referencePose!=ordered)return 131;
        const auto measured=measureReferencePoseDirections(orderedAuthoring.get().skeleton(),ordered,
            std::array{PoseReferenceDirectionQuery{2,1}});
        if(measured.size()!=1||std::abs(measured[0].x)>1e-10||measured[0].y<=0||std::abs(measured[0].z)>1e-10)return 135;
        Pose sourceReference(source.get().skeleton()),moving(source.get().skeleton()),targetReference(source.get().skeleton());
        auto local=moving.locals()[2];local.rotation=Quaternion::fromAxisAngle({0,0,1},.2);moving.setLocal(2,local);
        const auto transferred=retargetPose(sourceReference,moving,targetReference,std::array{PoseRetargetJoint{2,2}});
        if(transferred.size()!=3||std::abs(transferred[2].rotation.z)<.05)return 136;
        const AnimationClip motion(source.get().skeleton(),1.,{{2,{{0.,sourceReference.locals()[2]},{1.,local}}}});
        const auto baked=retargetAnimation(motion,sourceReference,targetReference,std::array{PoseRetargetJoint{2,2}},std::array{0.,.5,1.});
        Pose bakedPose(source.get().skeleton());baked.sample(.5,bakedPose);
        if(baked.tracks().size()!=3||std::abs(bakedPose.locals()[2].rotation.z)<.02)return 142;
        const auto animatedGlb=exportObject3DGlb(source.get(),baked);
        if(animatedGlb.size()<20||animatedGlb[0]!='g'||animatedGlb[1]!='l'||animatedGlb[2]!='T'||animatedGlb[3]!='F')return 143;
        const auto decodedGlb=GlbDocument::parse(animatedGlb);
        if(decodedGlb.accessorCount()==0||decodedGlb.bufferViewCount()==0||decodedGlb.readFloats(0).empty())return 155;
        auto defaultsData=data;defaultsData.referencePose.reset();defaultsData.meshes[0].morphDefaults={{"Shape",.3}};
        const auto defaultsSource=engine.createObject(std::move(defaultsData));
        const auto defaultsImport=importObject3DGlb(engine.reference(),exportObject3DGlb(defaultsSource.get()));
        const auto defaultsInstance=defaultsImport.object.get().instantiate();
        const auto *defaultsMesh=defaultsInstance->findComponent<SkeletalMeshComponent>();
        if(!defaultsMesh||defaultsMesh->morphWeights().size()!=1||std::abs(defaultsMesh->morphWeights()[0]-.3)>1e-6)return 156;
        auto defaultsEditor=engine.editObject(defaultsImport.object);defaultsEditor.get().setDefaultMorphWeight(0,"Shape",-.2);
        if(std::abs(defaultsEditor.get().object().get().data().meshes[0].morphDefaults[0].value+.2)>1e-10)return 157;
        auto names=engine.editObject(source);if(!names.get().renameJoints(std::array{JointRename{1,"arm"},JointRename{2,"tip"}}))return 139;
        if(names.get().object().get().data().bones[1].name!="arm"||!names.get().undo())return 140;
        Object3DPoseProgram program;program.joints={"root","arm","tip"};program.units={{"Adjust",{{},Quaternion::fromAxisAngle({0,0,1},.1),{}}}};
        const auto sampled=createReferenceObject3DPoseMorphs(authoring,program);if(sampled.get().data().referencePose||sampled.get().meshes()[0]->lods()[0].morphTargets.size()!=2)return 125;
        const auto poseRig=createObject3DPoseRig(authoring,program);const auto evaluation=poseRig.get().evaluate(std::array{.7});
        const auto rigSnapshot=createObject3DRigSnapshot(poseRig,evaluation);
        if(!engine.reference().owns(rigSnapshot)||rigSnapshot.get().data().referencePose)return 129;
        const auto linearRig=createObject3DRigSnapshot(poseRig,evaluation,{},Object3DSnapshotSkinning::LinearizeAtBind);
        if(!engine.reference().owns(linearRig)||linearRig.get().data().meshes[0].geometry.skinning.method!=SkinningMethod::Linear)return 151;
#if IIACTOROBJECT_HAS_NSCHARACTER
        using namespace iiActorObject::NSCharacter;
        const auto defaultView=std::make_shared<CharacterAsset>(defaultsImport.object);
        CharacterEditor defaultHumanEditor(defaultView);defaultHumanEditor.setDefaultMorphWeight(0,"Shape",.4);
        if(std::abs(defaultHumanEditor.asset()->data().meshes[0].morphDefaults[0].value-.4)>1e-10)return 158;
        const auto character=HumanoidGenerator::generate(engine.reference(),{}, {36,false,false});
        const auto mapping=mapHumanJoints(*character,humanJointRules(HumanSkeletonConvention::Procedural));
        const std::array rules{HumanJointDirection{"LeftArm","LeftHand",{1,0,0}}};const auto snapshot=createHumanPoseSnapshot(mapping,rules);
        if(!snapshot.valid()||!engine.reference().owns(snapshot.object())||snapshot.object().id()==character->object().id())return 113;
        const std::array frames{HumanJointFrame{"LeftHand","LeftHandMiddle1","LeftHandThumb1",{1,0,0},{0,-1,0}}};
        const auto hand=createHumanOrientedPoseSnapshot(mapping,rules,frames);if(!hand.valid()||!engine.reference().owns(hand.object()))return 117;
        CharacterEditor humanEdits(character);setHumanReferencePose(humanEdits,mapping,rules,frames);if(!humanEdits.asset()->data().referencePose||humanEdits.asset()->object().get().referencePoseLocals().empty())return 124;
        const std::array sequence{PoseReferenceStep{PoseReferenceStepKind::Direction,0},PoseReferenceStep{PoseReferenceStepKind::Frame,0}};
        const auto resolved=resolveHumanPoseReference(mapping,rules,frames,sequence);if(resolved.order.size()!=2)return 132;
        const auto orderedHuman=createHumanOrderedPoseSnapshot(mapping,rules,frames,sequence);if(!orderedHuman.valid())return 133;
        const auto automatic=planHumanReferencePose(mapping);if(automatic.directions.size()!=20||automatic.frames.size()!=2)return 134;
        const auto humanRetarget=planHumanPoseRetarget(mapping,mapping);if(humanRetarget.joints.empty()||!humanRetarget.skipped.empty())return 137;
        const auto creatorRules=humanJointRules(HumanSkeletonConvention::CharacterCreatorG3);
        if(creatorRules.size()!=53)return 138;
        const auto naming=planHumanJointNames(mapping,HumanSkeletonConvention::CharacterCreatorG3);CharacterEditor namingEditor(character);
        if(naming.changes.size()!=53||!applyHumanJointNames(namingEditor,mapping,naming))return 141;
#endif
    }
    if(held.valid()||deformedHeld.valid())return 114;
    std::cout<<"installedPoseSnapshot=1 baselineMorphSnapshot=1 referenceProgram=1 nativeVersionAtLeast13=1 sourceIdsRetained=1 morphInterpolation=1 coreOwned=1 expiredReference=1\n";return 0;
}
