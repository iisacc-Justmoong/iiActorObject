#pragma once
#include <iiActorObject/Object3DAutomaticCorrectives.h>
#include <iiActorObject/DualQuaternionSkinning.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/HumanAutomaticCorrectives.h>
#endif
#include <array>
#include <cmath>
#include <numbers>

inline int automaticCorrectiveConsumer()
{
    using namespace iiActorObject;
    Object3DData data;data.name="InstalledCorrective";
    data.bones={{"root",NoBone,{}},{"elbow",0,{}},{"hand",1,{{1,0,0}}}};
    Object3DMesh mesh;mesh.name="body";
    mesh.geometry.vertices={{{1,0,0},{0,0,1},{{0,.5},{1,.5}}},{{1,1,0},{0,0,1},{{0,.5},{1,.5}}},
                            {{0,1,0},{0,0,1},{{0,.5},{1,.5}}}};
    mesh.geometry.triangles={0,1,2};data.meshes.push_back(std::move(mesh));
    Object3DEngine engine;const auto source=engine.createObject(std::move(data));
    const auto q=Quaternion::fromAxisAngle({0,0,1},std::numbers::pi/2);
    const Object3DPoseCorrectiveProgram program{{{"elbow_fix",{{"elbow",q,1}},1}}};
    const auto generated=createObject3DAutomaticCorrectives(source,program);
    auto before=source.get().instantiate(),after=generated.object.get().instantiate();
    auto first=before->pose().locals()[1],second=after->pose().locals()[1];first.rotation=q;second.rotation=q;
    before->pose().setLocal(1,first);after->pose().setLocal(1,second);
    const auto rig=createObject3DPoseCorrectiveRig(generated.object,generated.program);
    applyObject3DPoseCorrectives(rig,*after);
    auto component=[](iiCharacter &actor){for(auto *part:actor.components())
        if(auto *value=dynamic_cast<SkeletalMeshComponent*>(part))return value;return static_cast<SkeletalMeshComponent*>(nullptr);};
    const auto original=component(*before),corrected=component(*after);if(!original||!corrected)return 1;
    std::vector<double> factors(original->mesh()->lods()[0].vertices.size(),1);
    std::vector<DeformedVertex> expected(factors.size());
    deformDualQuaternionSkin(*original->mesh(),before->pose(),0,original->morphWeights(),factors,expected);
    const auto actual=corrected->deformedVertices();
    for(std::size_t i=0;i<actual.size();++i)
        if(std::hypot(actual[i].position.x-expected[i].position.x,actual[i].position.y-expected[i].position.y,
                      actual[i].position.z-expected[i].position.z)>1e-8)return 2;
#if IIACTOROBJECT_HAS_NSCHARACTER
    using namespace iiActorObject::NSCharacter;
    const CharacterAsset character(source);
    const std::array rules{HumanJointRule{"LeftForeArm","elbow"},HumanJointRule{"LeftHand","hand"}};
    const auto mapping=mapHumanJoints(character,rules);const auto plan=planHumanAutomaticCorrectives(mapping);
    if(plan.program.samples.size()!=1||plan.skipped.size()!=3||!createHumanAutomaticCorrectives(plan).object.valid())return 3;
    HumanAutomaticCorrectiveOptions options;options.samplesPerJoint=3;
    const auto dense=planHumanAutomaticCorrectives(mapping,options);
    if(dense.program.samples.size()!=3||!createHumanAutomaticCorrectives(dense).object.valid())return 4;
#endif
    return 0;
}
