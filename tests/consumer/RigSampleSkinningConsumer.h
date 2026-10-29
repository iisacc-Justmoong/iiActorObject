#pragma once
#include <iiActorObject/Features.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DPoseRig.h>
#include <iiActorObject/Object3DRigSamples.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/RigSamples.h>
#endif
#include <array>
#include <cmath>

inline int rigSampleSkinningConsumer()
{
    using namespace iiActorObject;
    auto data=parseObject3DObj("v 1 0 0\nv 2 0 0\nv 1 1 0\nf 1 2 3\n");
    data.kind=Object3DKind::SkeletalMesh;data.bones={{"root",NoBone,{}},{"joint",0,{}}};
    auto &topology=*data.meshes[0].topology;
    for(auto &point:topology.points)point.weights={{0,.5},{1,.5}};
    topology.skinning={SkinningMethod::Blended,{.25,.5,1}};
    Object3DEngine engine;const auto source=engine.createObject(std::move(data));
    Object3DPoseProgram program;program.joints={"root","joint"};
    program.units={{"Turn",{Quaternion{},Quaternion::fromAxisAngle({0,0,1},.8)}}};
    const auto rig=createObject3DPoseRig(source,program);
    const RigSampleInput baseline{RigControlSpace::Raw,{{"Turn",.2}}};
    const RigSampleInput target{RigControlSpace::Raw,{{"Turn",1}}};
    const std::array requests{RigMorphRequest{"TurnSample",target}};
    RigMorphSampleOptions options;const auto policy=Object3DSnapshotSkinning::LinearizeAtBind;
#if IIACTOROBJECT_HAS_NSCHARACTER
    NSCharacter::CharacterAsset view(source);
    const auto wrapped=NSCharacter::createCharacterRigMorphSamples(view,rig,baseline,requests,options,policy);
    const auto result=wrapped->object();
#else
    const auto result=createObject3DRigMorphSamples(rig,baseline,requests,options,policy);
#endif
    if(result.get().data().meshes[0].topology->skinning.method!=SkinningMethod::Linear||
       source.get().data().meshes[0].topology->skinning.method!=SkinningMethod::Blended||
       exportObject3DGlb(result.get()).empty())return 1;
    const auto evaluation=rig.get().evaluateNamed(std::array{NamedRigControl{"Turn",1}},RigControlSpace::Raw,0);
    auto actor=source.get().instantiate();rig.get().applyTo(*actor,evaluation);
    const SkeletalMeshComponent *part=nullptr;
    for(const auto *component:actor->components())
        if(const auto *candidate=dynamic_cast<const SkeletalMeshComponent*>(component))part=candidate;
    if(!part)return 2;
    const auto expected=part->deformedVertices();const auto &mesh=*result.get().meshes()[0];
    const auto &geometry=mesh.lods()[0];std::vector<double> weights(geometry.morphTargets.size());bool found=false;
    for(std::size_t i=0;i<weights.size();++i)if(geometry.morphTargets[i].name=="TurnSample")
        {weights[i]=1;found=true;}
    if(!found)return 3;
    Pose rest(result.get().skeleton());std::vector<DeformedVertex> actual(expected.size());mesh.deform(rest,0,weights,actual);
    for(std::size_t i=0;i<actual.size();++i)
        if(std::hypot(actual[i].position.x-expected[i].position.x,actual[i].position.y-expected[i].position.y,
                      actual[i].position.z-expected[i].position.z)>1e-8)return 4;
    return 0;
}
