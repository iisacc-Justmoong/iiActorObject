#include <iiActorObject/Object3DPoseRig.h>
#include <iiActorObject/Object3DPoseMorphs.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DObj.h>
#include <array>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
void near(Vector3 a,Vector3 b){CHECK(std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)<2e-10);}
void near(Quaternion a,Quaternion b){CHECK(1-std::abs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w)<2e-12);}
constexpr std::string_view motion=R"(HIERARCHY
ROOT root { OFFSET 0 0 0 CHANNELS 6 Xposition Yposition Zposition Zrotation Xrotation Yrotation
 JOINT tip { OFFSET 0 1 0 CHANNELS 3 Xrotation Zrotation Yrotation End Site { OFFSET 0 1 0 } }
}
MOTION
Frames: 2
Frame Time: 0.04
0 0 0 0 0 0 10 0 0
0 0 0 0 0 0 40 0 0
)";
Object3DPoseProgram program()
{return makeObject3DPoseProgram(parseObject3DBvh(motion),std::array{PoseUnitFrame{"Close",1}});}
Object3DData data()
{
    auto d=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 3 3 1\nf 1 2 3 4\n");
    d.bones={{"root",NoBone,{{},Quaternion::fromAxisAngle({0,0,1},.25)}},{"tip",0,{{0,1,0},Quaternion::fromAxisAngle({0,1,0},.2)}}};
    for(auto &p:d.meshes[0].topology->points)p.weights={{1,1}};d.meshes[0].topology->points[4].weights.clear();return d;
}
std::unique_ptr<Pose> applied(const Object3DAsset &asset,const Object3DRigEvaluation &evaluation)
{auto pose=std::make_unique<Pose>(asset.skeleton());for(std::size_t i=0;i<evaluation.joints.size();++i)pose->setLocal(i,evaluation.joints[i]);return pose;}
void bvh()
{
    const auto b=parseObject3DBvh(motion);CHECK(b.joints().size()==2&&b.frameCount()==2&&b.frameTime()==.04);near(*b.joints()[1].endSite,{0,1,0});near(b.frame(1)[1].rotation,Quaternion::fromAxisAngle({1,0,0},40*std::numbers::pi/180));
    BvhImportOptions options;options.scale=.1;options.basis=Quaternion::fromAxisAngle({1,0,0},-std::numbers::pi/2);const auto converted=parseObject3DBvh(motion,options);near(converted.joints()[1].offset,{0,0,-.1});
    rejects([&]{(void)b.frame(2);});rejects([&]{(void)parseObject3DBvh(std::string(motion)+"0");});options.maximumSamples=2;rejects([&]{(void)parseObject3DBvh(motion,options);});
    const auto ordered=parseObject3DBvh("HIERARCHY ROOT r { OFFSET 0 0 0 CHANNELS 3 Zrotation Xrotation Yrotation End Site { OFFSET 0 1 0 } } MOTION Frames: 1 Frame Time: .04 90 90 0");
    near(ordered.frame(0)[0].matrix().transformVector({0,1,0}),{0,0,1});
    auto signedText=std::string("\xef\xbb\xbf")+std::string(motion);const auto number=signedText.find("Frame Time: 0.04");signedText.replace(number,16,"Frame Time: +.04");CHECK(parseObject3DBvh(signedText).frameCount()==2);
    auto invalid=std::string(motion);invalid.replace(invalid.find("0.04"),4,"+-0.04");rejects([&]{(void)parseObject3DBvh(invalid);});
    const auto cleaned=makeObject3DPoseProgram(b,std::array{PoseUnitFrame{"Close",1}},0,1);near(cleaned.units[0].worldRotationDeltas[1],{});
}
void evaluate()
{
    Object3DEngine engine;auto object=engine.createObject(data());const auto p=program();near(p.units[0].worldRotationDeltas[1],Quaternion::fromAxisAngle({1,0,0},std::numbers::pi/6));auto rig=createObject3DPoseRig(object,p);
    CHECK(rig.get().jointMapping().size()==2);const auto evaluation=rig.get().evaluateNamed(std::array{NamedRigControl{"Close",1}});auto pose=applied(object.get(),evaluation);Pose rest(object.get().skeleton());
    const auto expected=Transform{{},p.units[0].worldRotationDeltas[1]}.matrix()*rest.worldMatrices()[1];
    for(const auto axis:std::array<Vector3,3>{{{1,0,0},{0,1,0},{0,0,1}}})near(pose->worldMatrices()[1].transformVector(axis),expected.transformVector(axis));
    const auto reset=rig.get().evaluateNamed({});for(std::size_t i=0;i<reset.joints.size();++i)near(reset.joints[i].rotation,object.get().data().bones[i].bindTransform.rotation);
    rejects([&]{(void)rig.get().evaluateNamed(std::array{NamedRigControl{"Close",1.1}});});
}
void clone()
{
    Object3DEngine engine;auto object=engine.createObject(data());auto rig=createObject3DPoseRig(object,program());auto d=object.get().data();d.bones[1].bindTransform.rotation=Quaternion::fromAxisAngle({0,0,1},.7);d.bones[1].name="Edited tip";auto edited=engine.createObject(d);
    const std::array<BoneIndex,2> mapping{0,1};auto rebound=rig.get().cloneFor(edited,mapping);auto e=rebound.get().evaluateNamed(std::array{NamedRigControl{"Close",1}});Pose rest(edited.get().skeleton());auto pose=applied(edited.get(),e);const auto expected=Transform{{},program().units[0].worldRotationDeltas[1]}.matrix()*rest.worldMatrices()[1];
    near(pose->worldMatrices()[1].transformVector({0,1,0}),expected.transformVector({0,1,0}));CHECK(rig.get().sourceBytes()==rebound.get().sourceBytes());
}
void overlay()
{
    Object3DEngine engine;auto object=engine.createObject(data());auto rig=createObject3DPoseRig(object,program());
    CHECK(rig.get().definition().drivenJoints&&rig.get().definition().drivenJoints->size()==1);
    auto actor=object.get().instantiate();auto body=actor->pose().locals()[0];body.position.x=2;body.rotation=Quaternion::fromAxisAngle({0,0,1},.6);actor->pose().setLocal(0,body);
    rig.get().applyTo(*actor,rig.get().evaluateNamed(std::array{NamedRigControl{"Close",1}}));near(actor->pose().locals()[0].position,body.position);near(actor->pose().locals()[0].rotation,body.rotation);
    rig.get().applyTo(*actor,rig.get().evaluateNamed({}));near(actor->pose().locals()[1].rotation,object.get().data().bones[1].bindTransform.rotation);near(actor->pose().locals()[0].rotation,body.rotation);
}
void codec()
{
    auto p=program();auto bytes=exportObject3DPoseProgram(p);auto restored=importObject3DPoseProgram(bytes);CHECK(restored.joints==p.joints&&restored.units[0].name=="Close");near(restored.units[0].worldRotationDeltas[1],p.units[0].worldRotationDeltas[1]);
    bytes.back()^=std::byte{1};rejects([&]{(void)importObject3DPoseProgram(bytes);});rejects([&]{(void)exportObject3DPoseProgram(p,24);});p.units[0].worldRotationDeltas.pop_back();rejects([&]{validateObject3DPoseProgram(p);});
}
void bake()
{
    Object3DEngine engine;auto object=engine.createObject(data());auto rig=createObject3DPoseRig(object,program());auto pose=applied(object.get(),rig.get().evaluateNamed(std::array{NamedRigControl{"Close",1}}));const auto n=object.get().meshes()[0]->lods()[0].vertices.size();std::vector<DeformedVertex> expected(n),actual(n);object.get().meshes()[0]->deform(*pose,0,{},expected);
    auto d=object.get().data();const auto r=bakeObject3DPoseMorphs(d,program());CHECK(r.morphs==1&&r.positionDeltas>0&&r.normalDeltas>0);CHECK(d.meshes[0].topology->morphTargets[0].normalDeltas.has_value());
    for(const auto &delta:d.meshes[0].topology->morphTargets[0].deltas)CHECK(delta.point!=4);
    auto baked=engine.createObject(d);Pose bind(baked.get().skeleton());baked.get().meshes()[0]->deform(bind,0,std::array{1.},actual);
    for(std::size_t i=0;i<n;++i){near(actual[i].position,expected[i].position);near(actual[i].normal,expected[i].normal);}
    auto runtimeRig=createObject3DPoseRig(baked,program());auto actor=baked.get().instantiate();
    for(auto *component:actor->components())if(auto *mesh=dynamic_cast<SkeletalMeshComponent*>(component))mesh->setMorphWeight(0,1);
    runtimeRig.get().applyTo(*actor,runtimeRig.get().evaluateNamed(std::array{NamedRigControl{"Close",1}}));
    for(auto *component:actor->components())if(auto *mesh=dynamic_cast<SkeletalMeshComponent*>(component)){CHECK(mesh->morphWeights()[0]==0);for(std::size_t i=0;i<n;++i)near(mesh->deformedVertices()[i].position,expected[i].position);}
    rejects([&]{(void)bakeObject3DPoseMorphs(d,program());});PoseMorphBakeOptions options;options.replaceExisting=true;CHECK(bakeObject3DPoseMorphs(d,program(),{},options).morphs==1);
    auto limited=object.get().data();options.maximumDeltas=1;rejects([&]{(void)bakeObject3DPoseMorphs(limited,program(),{},options);});CHECK(limited.meshes[0].topology->morphTargets.empty());
    auto staticObject=engine.createObject(parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));Object3DPoseProgram staticProgram;
    staticProgram.joints={staticObject.get().data().bones[0].name};staticProgram.units={{"Rotate",{Quaternion::fromAxisAngle({1,0,0},.3)}}};
    auto staticData=staticObject.get().data();CHECK(bakeObject3DPoseMorphs(staticData,staticProgram).positionDeltas>0);
}
void bakeDualQuaternion()
{
    Object3DEngine engine;auto sourceData=data();auto &topology=*sourceData.meshes[0].topology;
    for(auto &point:topology.points)point.weights={{0,.5},{1,.5}};
    topology.skinning={SkinningMethod::Blended,{1,.5,.75,0,1}};
    const auto object=engine.createObject(sourceData);const auto rig=createObject3DPoseRig(object,program());
    const auto pose=applied(object.get(),rig.get().evaluateNamed(std::array{NamedRigControl{"Close",1}}));
    const auto &mesh=*object.get().meshes()[0];std::vector<DeformedVertex> expected(mesh.lods()[0].vertices.size());
    mesh.deform(*pose,0,{},expected);auto bakedData=object.get().data();(void)bakeObject3DPoseMorphs(bakedData,program());
    const auto baked=engine.createObject(bakedData);Pose rest(baked.get().skeleton());std::vector<DeformedVertex> actual(expected.size());
    baked.get().meshes()[0]->deform(rest,0,std::array{1.},actual);
    for(std::size_t i=0;i<actual.size();++i){near(actual[i].position,expected[i].position);near(actual[i].normal,expected[i].normal);}
    // The hidden weighted control point must follow the same DQ transform.
    const auto &before=*object.get().data().meshes[0].topology,&after=*baked.get().data().meshes[0].topology;
    SkeletalMeshLod isolated;isolated.vertices={{before.points[4].position,{0,0,1},before.points[4].weights}};
    isolated.skinning={SkinningMethod::Blended,{before.skinning.blendWeights[4]}};
    SkeletalMesh point(object.get().skeleton(),{std::move(isolated)});std::array<DeformedVertex,1> reference;
    point.deform(*pose,0,{},reference);Vector3 delta{};
    for(const auto &entry:after.morphTargets[0].deltas)if(entry.point==4)delta=entry.position;
    near({after.points[4].position.x+delta.x,after.points[4].position.y+delta.y,after.points[4].position.z+delta.z},reference[0].position);
}
void project()
{
    Object3DEngine engine;Object3DTemplateDefinition definition;definition.base=engine.createObject(data());definition.poseProgram=program();auto editor=engine.editTemplate(engine.createTemplate(definition));editor.get().moveControlPoints(0,std::array{TopologyPointMove{0,{.001,0,0}}});
    auto rig=createObject3DPoseRig(editor.get().object(),program());const auto expected=rig.get().evaluateNamed(std::array{NamedRigControl{"Close",.7}});auto bytes=exportObject3DProject(editor,std::array{rig});CHECK(bytes[8]==8);
    Object3DEngine destination;auto restored=importObject3DProject(destination.reference(),bytes);CHECK(restored.rigs.size()==1&&restored.editor.get().templateProgram().get().definition().poseProgram.has_value());const auto actual=restored.rigs[0].get().evaluateNamed(std::array{NamedRigControl{"Close",.7}});
    for(std::size_t i=0;i<actual.joints.size();++i){near(actual.joints[i].position,expected.joints[i].position);near(actual.joints[i].rotation,expected.joints[i].rotation);}CHECK(restored.editor.get().undo()&&restored.editor.get().redo());
}
int main(int argc,char **argv)
{
    try{if(argc!=2)return 2;const std::string_view mode=argv[1];if(mode=="bvh")bvh();else if(mode=="evaluate")evaluate();else if(mode=="clone")clone();else if(mode=="overlay")overlay();else if(mode=="codec")codec();else if(mode=="bake")bake();else if(mode=="bake_dual_quaternion")bakeDualQuaternion();else if(mode=="project")project();else return 2;std::cout<<mode<<" passed\n";}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
