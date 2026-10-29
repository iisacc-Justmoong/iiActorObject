#include <iiActorObject/Object3DInstanceTransfer.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProxy.h>
#include <iiActorObject/Object3DPoseRig.h>
#include <iiActorObject/Object3DTemplateMorphs.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class F>void rejects(F action){bool failed=false;try{action();}catch(const std::exception&){failed=true;}CHECK(failed);}
bool near(double a,double b){return std::abs(a-b)<1e-9;}
void sameMatrix(const Matrix4 &a,const Matrix4 &b)
{for(std::size_t i=0;i<16;++i)CHECK(near(a.elements()[i],b.elements()[i]));}
Object3DData data()
{
    Object3DData value;value.name="Character";
    value.bones={{"root",NoBone,{}},{"jaw",0,{{0,1,0}}},{"tip",1,{{0,.5,0}}}};
    value.semantics={{"Root",0},{"Jaw",1},{"Tip",2}};
    Object3DMesh body;body.name="Body";
    body.geometry.vertices={{{0,1,0},{0,0,1},{{1,1}}},{{1,1,0},{0,0,1},{{1,1}}},{{0,2,0},{0,0,1},{{1,1}}}};
    body.geometry.triangles={0,1,2};
    body.geometry.morphTargets={{"Smile",{{0,{0,.2,0},{}}}},{"Blink",{{1,{0,0,.1},{}}}}};
    auto hair=body;hair.name="Hair";hair.geometry.morphTargets.clear();
    value.meshes={body,hair};return value;
}
SkeletalMeshComponent &part(iiCharacter &actor,std::string_view name)
{
    for(auto *component:actor.components())if(auto *mesh=dynamic_cast<SkeletalMeshComponent*>(component);mesh&&mesh->name()==name)return *mesh;
    throw std::runtime_error("Missing component");
}
std::shared_ptr<const AnimationClip> clip(const Object3DHandle &source,std::vector<Transform> bind={})
{
    auto start=source.get().data().bones[1].bindTransform,finish=start;finish.position.y+=.5;
    finish.rotation=Quaternion::fromAxisAngle({0,0,1},.4);finish.scale={1.2,1.2,1.2};
    return std::make_shared<AnimationClip>(source.get().skeleton(),2,std::vector<BoneTrack>{{1,{{0,start},{2,finish}}}},
        std::vector<AnimationCurve>{{"custom",{{0,0},{2,1}}}},std::move(bind));
}
void animated()
{
    Object3DEngine engine;auto initial=data();initial.bones[1].bindTransform.rotation=Quaternion::fromAxisAngle({0,0,1},.1);
    const auto source=engine.createObject(std::move(initial));auto old=source.get().instantiate("Live");
    old->setPosition({2,3,4});CHECK(old->addTag("selected"));old->setTickEnabled(false);
    old->rootComponent().setName("Pivot");old->rootComponent().setActive(false);old->rootComponent().setTickEnabled(false);
    auto &body=part(*old,"Body");body.setMorphWeight(0,.7);body.setMorphWeight(1,.3);
    body.setLocalTransform({{.1,.2,.3}});body.setActive(false);body.setTickEnabled(false);
    old->animation().play(clip(source),AnimationWrap::Clamp);old->animation().seek(.75);old->animation().setRate(-.5);old->animation().pause();
    auto pose=old->pose().locals()[1];pose.position.x+=.1;old->pose().setLocal(1,pose);
    auto next=source.get().data();next.bones[1].name="renamed_jaw";next.bones[1].bindTransform.position.y=2;
    next.bones[1].bindTransform.rotation=Quaternion::fromAxisAngle({0,0,1},.2);next.bones[1].bindTransform.scale={1.2,1.2,1.2};
    std::swap(next.meshes[0].geometry.morphTargets[0],next.meshes[0].geometry.morphTargets[1]);
    next.meshes[0].geometry.morphTargets.push_back({"New",{{2,{.1,0,0},{}}}});next.meshes[0].morphDefaults={{"New",.4}};
    next.meshes[1].name="Hat";const auto target=engine.createObject(std::move(next));
    auto result=transferObject3DInstance(source,*old,target);auto &migrated=*result.actor;
    CHECK(result.report.matchedMeshes==1&&result.report.addedMeshes==1&&result.report.removedMeshes==1&&result.report.copiedAnimation);
    CHECK(migrated.name()=="Live"&&migrated.position()==old->position()&&migrated.hasTag("selected")&&!migrated.isTickEnabled());
    CHECK(migrated.rootComponent().name()=="Pivot"&&!migrated.rootComponent().isActive()&&!migrated.rootComponent().isTickEnabled());
    CHECK(migrated.state()==LifecycleState::Created&&migrated.animation().clip()&&migrated.animation().wrap()==AnimationWrap::Clamp);
    CHECK(near(migrated.animation().time(),.75)&&near(migrated.animation().rate(),-.5)&&!migrated.animation().isPlaying());
    CHECK(near(migrated.pose().locals()[1].position.x,.1)&&near(migrated.pose().locals()[1].position.y,pose.position.y+1));
    CHECK(near(migrated.pose().locals()[1].scale.x,pose.scale.x*1.2));
    CHECK(near(migrated.pose().locals()[1].rotation.z,std::sin(.3125/2)));
    auto &output=part(migrated,"Body");CHECK(!output.isActive()&&!output.isTickEnabled()&&output.localTransform()==body.localTransform());
    CHECK(near(output.morphWeights()[0],.3)&&near(output.morphWeights()[1],.7)&&near(output.morphWeights()[2],.4));
    CHECK(old->animation().time()==.75&&old->pose().locals()[1]==pose);
    old->animation().resume();migrated.animation().resume();CHECK(old->animation().advance(.1)&&migrated.animation().advance(.1));
    CHECK(near(migrated.animation().time(),old->animation().time()));
    CHECK(near(migrated.pose().locals()[1].position.y,old->pose().locals()[1].position.y+1));
    CHECK(near(migrated.pose().locals()[1].scale.x,old->pose().locals()[1].scale.x*1.2));
    CHECK(near(migrated.pose().locals()[1].rotation.z,std::sin(.305/2)));
    CHECK(near(migrated.animation().clip()->sampleCurve(0,.7),old->animation().clip()->sampleCurve(0,.7)));
}
void world()
{
    Object3DEngine engine;const auto source=engine.createObject(data());auto actor=source.get().instantiate();
    auto jaw=actor->pose().locals()[1];jaw.rotation=Quaternion::fromAxisAngle({0,0,1},.5);actor->pose().setLocal(1,jaw);
    auto tip=actor->pose().locals()[2];tip.rotation=Quaternion::fromAxisAngle({1,0,0},.2);actor->pose().setLocal(2,tip);
    const auto world=actor->pose().worldMatrices();std::vector<Matrix4> expected(world.begin(),world.end());
    auto changed=source.get().data();changed.bones[2].parent=0;const auto target=engine.createObject(std::move(changed));
    rejects([&]{(void)transferObject3DInstance(source,*actor,target);});
    Object3DInstanceTransferOptions options;options.pose=Object3DInstancePoseTransfer::PreserveWorld;
    const auto result=transferObject3DInstance(source,*actor,target,options);
    const auto actual=result.actor->pose().worldMatrices();for(std::size_t i=0;i<actual.size();++i)sameMatrix(actual[i],expected[i]);
    actor->animation().play(clip(source));rejects([&]{(void)transferObject3DInstance(source,*actor,target,options);});
    options.discardAnimation=true;const auto without=transferObject3DInstance(source,*actor,target,options);
    CHECK(without.report.discardedAnimation&&!without.actor->animation().clip());
    auto shearedData=data();shearedData.bones[1].bindTransform.scale={2,1,1};
    shearedData.bones[2].bindTransform.rotation=Quaternion::fromAxisAngle({0,0,1},.4);
    const auto shearSource=engine.createObject(shearedData);auto shearActor=shearSource.get().instantiate();
    shearedData.bones[2].parent=0;const auto shearTarget=engine.createObject(std::move(shearedData));
    rejects([&]{(void)transferObject3DInstance(shearSource,*shearActor,shearTarget,options);});
}
void safeguards()
{
    Object3DEngine engine;const auto source=engine.createObject(data());auto actor=source.get().instantiate();
    part(*actor,"Body").setMorphWeight(0,.8);auto changed=source.get().data();
    changed.meshes[0].geometry.morphTargets.erase(changed.meshes[0].geometry.morphTargets.begin());
    const auto missing=engine.createObject(changed);const auto objects=engine.objectCount();
    rejects([&]{(void)transferObject3DInstance(source,*actor,missing);});
    CHECK(engine.objectCount()==objects&&near(part(*actor,"Body").morphWeights()[0],.8));
    Object3DInstanceTransferOptions options;options.discardMissingMorphs=true;
    const auto allowed=transferObject3DInstance(source,*actor,missing,options);CHECK(allowed.report.discardedMorphs==1);
    actor->createComponent<SceneComponent>("Custom");rejects([&]{(void)transferObject3DInstance(source,*actor,source);});
    options.discardExtraComponents=true;const auto dropped=transferObject3DInstance(source,*actor,source,options);
    CHECK(dropped.report.discardedComponents==1);
    auto second=source.get().instantiate();part(*second,"Body").setDualQuaternionBlendWeights(std::array{0.,.5,1.});
    const auto unchanged=transferObject3DInstance(source,*second,source);
    CHECK(unchanged.actor->findComponent<SkeletalMeshComponent>()->dualQuaternionBlendWeights().size()==3);
    changed=source.get().data();changed.meshes[0].geometry.vertices[0].position.x+=.1;const auto edited=engine.createObject(changed);
    rejects([&]{(void)transferObject3DInstance(source,*second,edited);});
    options={};options.discardIncompatibleSkinOverrides=true;
    const auto without=transferObject3DInstance(source,*second,edited,options);CHECK(without.report.discardedSkinOverrides==1);
    auto duplicate=data();duplicate.meshes[0].geometry.vertices.push_back(duplicate.meshes[0].geometry.vertices[0]);
    duplicate.meshes[0].geometry.triangles.insert(duplicate.meshes[0].geometry.triangles.end(),{3,1,2});
    const auto ambiguousSource=engine.createObject(duplicate),ambiguousTarget=engine.createObject(duplicate);auto ambiguous=ambiguousSource.get().instantiate();
    part(*ambiguous,"Body").setDualQuaternionBlendWeights(std::array{0.,.5,1.,.75});
    CHECK(transferObject3DInstance(ambiguousSource,*ambiguous,ambiguousSource).report.copiedSkinOverrides==1);
    rejects([&]{(void)transferObject3DInstance(ambiguousSource,*ambiguous,ambiguousTarget);});
    auto other=engine.createObject(data());rejects([&]{(void)transferObject3DInstance(other,*second,source);});
    auto semantic=source.get().data();semantic.semantics[1].bone=0;const auto mismatched=engine.createObject(std::move(semantic));
    rejects([&]{(void)transferObject3DInstance(source,*second,mismatched);});
    auto invalid=Object3DInstanceTransferOptions{};invalid.matrixTolerance=0;
    rejects([&]{(void)transferObject3DInstance(source,*second,source,invalid);});
    auto destroyed=source.get().instantiate();CHECK(destroyed->destroy());
    rejects([&]{(void)transferObject3DInstance(source,*destroyed,source);});
}
void deformationBind()
{
    Object3DEngine engine;const auto source=engine.createObject(data());auto actor=source.get().instantiate();
    std::vector<Transform> bind;for(const auto &bone:source.get().data().bones)bind.push_back(bone.bindTransform);bind[1].position.y+=.2;
    CHECK(actor->pose().setDeformationBind(bind));actor->animation().play(clip(source,bind),AnimationWrap::Loop);actor->animation().seek(.8);
    auto targetData=source.get().data();targetData.meshes[0].geometry.vertices[0].position.z+=.01;
    const auto target=engine.createObject(targetData);auto restored=transferObject3DInstance(source,*actor,target);
    CHECK(restored.actor->pose().deformationBindTransforms().size()==bind.size()&&near(restored.actor->animation().time(),.8));
    CHECK(restored.actor->animation().wrap()==AnimationWrap::Loop&&restored.actor->animation().isPlaying());
    targetData.bones[1].bindTransform.position.y+=.5;const auto changed=engine.createObject(targetData);
    rejects([&]{(void)transferObject3DInstance(source,*actor,changed);});
    Object3DInstanceTransferOptions options;options.discardIncompatibleDeformationBind=true;options.discardAnimation=true;
    const auto dropped=transferObject3DInstance(source,*actor,changed,options);
    CHECK(dropped.report.discardedAnimation&&dropped.report.discardedDeformationBind&&dropped.actor->pose().deformationBindTransforms().empty());
    auto errored=source.get().instantiate();CHECK(errored->pose().setDeformationBind(bind));
    errored->animation().play(clip(source,bind));CHECK(errored->pose().clearDeformationBind());
    CHECK(errored->beginPlay()&&errored->tick(.1)&&errored->animationError()&&!errored->animation().isPlaying());
    Object3DInstanceTransferOptions errorOptions;errorOptions.discardAnimation=true;
    const auto withoutError=transferObject3DInstance(source,*errored,source,errorOptions);
    CHECK(withoutError.report.omittedAnimationError&&withoutError.report.discardedAnimation&&!withoutError.actor->animationError());
}
void scene()
{
    Object3DEngine engine;const auto source=engine.createObject(data());auto actor=source.get().instantiate();
    actor->setTransform({{1,2,3},Quaternion::fromAxisAngle({0,0,1},.2)});
    auto &hair=part(*actor,"Hair"),&body=part(*actor,"Body");
    hair.setLocalTransform({{.5,.2,0},Quaternion::fromAxisAngle({0,1,0},.3)});
    body.setLocalTransform({{0,.3,.4},Quaternion::fromAxisAngle({1,0,0},.2)});
    CHECK(body.attachTo(&hair));const auto expected=body.worldMatrix();
    auto changed=source.get().data();changed.meshes.erase(changed.meshes.begin()+1);const auto target=engine.createObject(changed);
    rejects([&]{(void)transferObject3DInstance(source,*actor,target);});
    Object3DInstanceTransferOptions options;options.discardExtraComponents=true;
    auto transferred=transferObject3DInstance(source,*actor,target,options);
    CHECK(transferred.report.reattachedComponents==1&&part(*transferred.actor,"Body").parent()==&transferred.actor->rootComponent());
    sameMatrix(part(*transferred.actor,"Body").worldMatrix(),expected);
    auto hidden=source.get().instantiate();auto &removed=part(*hidden,"Hair");CHECK(hidden->removeComponent(removed));
    const auto suppressed=transferObject3DInstance(source,*hidden,source);
    CHECK(suppressed.report.suppressedMeshes==1&&suppressed.actor->componentCount()==2);
    auto attached=source.get().instantiate();auto &custom=attached->createComponent<SceneComponent>("Custom");
    custom.setLocalTransform({{0,.2,.3},Quaternion::fromAxisAngle({0,0,1},.3)});
    auto &attachedBody=part(*attached,"Body");CHECK(attachedBody.attachTo(&custom));const auto attachedWorld=attachedBody.worldMatrix();
    rejects([&]{(void)transferObject3DInstance(source,*attached,source);});
    const auto detached=transferObject3DInstance(source,*attached,source,options);
    CHECK(detached.report.discardedComponents==1&&detached.report.reattachedComponents==1);
    sameMatrix(part(*detached.actor,"Body").worldMatrix(),attachedWorld);
}
void topology()
{
    Object3DEngine engine;auto sourceData=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3\nf 1 3 4\n");
    sourceData.bones={{"root",NoBone,{}},{"jaw",0,{{0,1,0}}}};
    for(auto &point:sourceData.meshes[0].topology->points)point.weights={{1,1}};
    const auto source=engine.createObject(sourceData);auto actor=source.get().instantiate();
    auto *mesh=actor->findComponent<SkeletalMeshComponent>();CHECK(mesh);mesh->setDualQuaternionBlendWeights(std::array{0.,.25,.5,1.});
    auto edited=source.get().data();edited.meshes[0].topology->points[1].position.z=.1;
    const auto target=engine.createObject(std::move(edited));const auto transferred=transferObject3DInstance(source,*actor,target);
    const auto *newMesh=transferred.actor->findComponent<SkeletalMeshComponent>();CHECK(newMesh);
    CHECK(transferred.report.copiedSkinOverrides==1&&newMesh->dualQuaternionBlendWeights().size()==4&&newMesh->dualQuaternionBlendWeights()[1]==.25);
    auto hidden=source.get().data();CHECK(hideObject3DProxyCoveredPolygons(*hidden.meshes[0].topology,std::array<std::uint32_t,3>{0,1,2})==1);
    const auto covered=engine.createObject(std::move(hidden));rejects([&]{(void)transferObject3DInstance(source,*actor,covered);});
    Object3DInstanceTransferOptions options;options.discardIncompatibleSkinOverrides=true;
    const auto without=transferObject3DInstance(source,*actor,covered,options);CHECK(without.report.discardedSkinOverrides==1);
}
void synchronized()
{
    Object3DEngine engine;const auto source=engine.createObject(data());auto actor=source.get().instantiate();
    part(*actor,"Body").setMorphWeight(0,.8);const auto editor=engine.editObject(source);
    Object3DPoseProgram program;program.joints={"root","jaw"};
    program.units={{"Mouth",{{},Quaternion::fromAxisAngle({0,0,1},.3)}}};
    const auto pose=createObject3DPoseRig(source,program),morph=createObject3DMorphChannelRig(source);
    Object3DEditorRigSet rigs(editor,std::array{pose,morph});CHECK(rigs.boundObject()==source);
    auto missing=source.get().data();missing.meshes[0].geometry.morphTargets.erase(missing.meshes[0].geometry.morphTargets.begin());
    editor.get().replace(std::move(missing));const auto count=engine.rigCount();
    rejects([&]{(void)transferObject3DEditorInstance(source,*actor,rigs);});
    CHECK(rigs.boundObject()==source&&engine.rigCount()==count&&near(part(*actor,"Body").morphWeights()[0],.8));
    Object3DInstanceTransferOptions options;options.discardMissingMorphs=true;
    rejects([&]{(void)transferObject3DEditorInstance(source,*actor,rigs,options);});
    CHECK(rigs.boundObject()==source&&engine.rigCount()==count&&pose.valid()&&morph.valid());
    CHECK(editor.get().undo());editor.get().moveJoint(1,{0,2,0});
    const auto transferred=transferObject3DEditorInstance(source,*actor,rigs);
    CHECK(transferred.report.rigsRebound&&rigs.boundObject()==editor.get().object());
    CHECK(transferred.actor->pose().skeleton()==editor.get().object().get().skeleton()&&actor->pose().skeleton()==source.get().skeleton());
    CHECK(rigs.rig(0).get().object()==editor.get().object()&&rigs.rig(1).get().object()==editor.get().object());
    rejects([&]{(void)transferObject3DEditorInstance(source,*actor,rigs);});
}
int main(int argc,char **argv)
{
    try{if(argc!=2)return 2;const std::string_view mode=argv[1];if(mode=="animated")animated();else if(mode=="world")world();
        else if(mode=="safeguards")safeguards();else if(mode=="deformation_bind")deformationBind();else if(mode=="scene")scene();else if(mode=="topology")topology();else if(mode=="synchronized")synchronized();else return 2;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
