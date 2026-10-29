#include <iiActorObject/Object3DPoseCorrectives.h>
#include <iiActorObject/Object3DPoseRig.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>

using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}CHECK(failed);}
bool near(double a,double b,double epsilon=1e-9){return std::abs(a-b)<epsilon;}
std::uint32_t crc(std::span<const std::uint8_t> bytes)
{
    std::uint32_t result=0xffffffffu;
    for(auto byte:bytes){result^=byte;for(unsigned i=0;i<8;++i)result=(result>>1)^((result&1)?0xedb88320u:0u);}return ~result;
}
Quaternion multiply(Quaternion a,Quaternion b)
{return Quaternion{a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
    a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z}.normalized();}
constexpr double HalfPi=std::numbers::pi/2;
void rotate(Pose &pose,BoneIndex bone,Vector3 axis,double angle)
{
    auto value=pose.skeleton()->bones()[bone].bindTransform;
    value.rotation=multiply(value.rotation,Quaternion::fromAxisAngle(axis,angle));pose.setLocal(bone,value);
}
Object3DData data()
{
    auto value=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n");
    value.kind=Object3DKind::SkeletalMesh;value.bones={{"root",NoBone,{{},Quaternion::fromAxisAngle({0,0,1},.2)}},
        {"elbow",0,{{0,1,0},Quaternion::fromAxisAngle({0,1,0},.3)}},{"wrist",1,{{0,1,0}}}};
    auto &mesh=value.meshes[0];mesh.name="body";for(auto &p:mesh.topology->points)p.weights={{1,1}};
    mesh.topology->morphTargets={{"elbow_fix",{{0,{0,0,.2}}}},
                                {"combined",{{1,{.1,0,0}}}}, {"keep",{{2,{0,0,.3}}}}};
    auto garment=mesh;garment.name="garment";value.meshes.push_back(std::move(garment));return value;
}
Object3DPoseCorrectiveProgram program()
{
    return {{{"elbow_fix",{{"elbow",Quaternion::fromAxisAngle({1,0,0},HalfPi),HalfPi}},1},
             {"elbow_fix",{{"elbow",Quaternion::fromAxisAngle({1,0,0},-HalfPi),HalfPi}},.6},
             {"elbow_fix",{{"elbow",Quaternion::fromAxisAngle({1,0,0},HalfPi),HalfPi}},.8},
             {"combined",{{"elbow",Quaternion::fromAxisAngle({1,0,0},HalfPi),HalfPi},
                           {"wrist",Quaternion::fromAxisAngle({0,1,0},std::numbers::pi/3),std::numbers::pi/3}},.8}}};
}
class SpoofedDriver final:public Object3DRigDriver
{
  public:
    SpoofedDriver()
    {definition_.sourceFormat="iiActorObject.poseCorrective.1";definition_.joints={"elbow"};
     definition_.rawControls={"x","y","z","w"};definition_.blendShapes={"elbow_fix"};}
    const Object3DRigDefinition &definition()const noexcept override{return definition_;}
    std::unique_ptr<Object3DRigDriver> clone()const override{return std::make_unique<SpoofedDriver>(*this);}
    Object3DRigOutputs evaluate(std::span<const double>,RigControlSpace,std::size_t)override
    {Object3DRigOutputs out;out.joints.resize(1);out.blendShapes={1};return out;}
  private:Object3DRigDefinition definition_;
};
std::vector<SkeletalMeshComponent*> meshes(iiCharacter &actor)
{
    std::vector<SkeletalMeshComponent*> result;
    for(auto *component:actor.components())if(auto *mesh=dynamic_cast<SkeletalMeshComponent*>(component))result.push_back(mesh);
    return result;
}
std::size_t morph(const SkeletalMeshComponent &mesh,std::string_view name)
{
    const auto &channels=mesh.mesh()->lods()[mesh.lod()].morphTargets;
    for(std::size_t i=0;i<channels.size();++i)if(channels[i].name==name)return i;
    throw std::runtime_error("Missing test morph");
}
void evaluate()
{
    Object3DEngine engine;const auto object=engine.createObject(data());const auto rig=createObject3DPoseCorrectiveRig(object,program());
    auto actor=object.get().instantiate();auto independent=object.get().instantiate();auto parts=meshes(*actor);CHECK(parts.size()==2);
    for(auto *part:parts)part->setMorphWeight(morph(*part,"keep"),.33);
    const auto neutral=evaluateObject3DPoseCorrectives(rig,actor->pose());
    CHECK(neutral.blendShapes.size()==2&&near(neutral.blendShapes[0],0)&&near(neutral.blendShapes[1],0));
    rotate(actor->pose(),1,{1,0,0},HalfPi/2);const auto half=evaluateObject3DPoseCorrectives(rig,actor->pose());
    CHECK(near(half.blendShapes[0],.5)&&near(half.blendShapes[1],0));
    const auto poseBefore=actor->pose().locals()[1];applyObject3DPoseCorrectives(rig,*actor);CHECK(actor->pose().locals()[1]==poseBefore);
    for(auto *part:parts){CHECK(near(part->morphWeights()[morph(*part,"elbow_fix")],.5));CHECK(near(part->morphWeights()[morph(*part,"keep")],.33));}
    for(auto *part:meshes(*independent))CHECK(near(part->morphWeights()[morph(*part,"elbow_fix")],0));
    (void)parts[0]->deformedVertices();const auto count=parts[0]->deformationCount();applyObject3DPoseCorrectives(rig,*actor);
    (void)parts[0]->deformedVertices();CHECK(parts[0]->deformationCount()==count);
    rotate(actor->pose(),1,{1,0,0},HalfPi);rotate(actor->pose(),2,{0,1,0},std::numbers::pi/3);
    const auto full=evaluateObject3DPoseCorrectives(rig,actor->pose());CHECK(near(full.blendShapes[0],1)&&near(full.blendShapes[1],.8));
    applyObject3DPoseCorrectives(rig,*actor);for(auto *part:parts)CHECK(near(part->morphWeights()[morph(*part,"combined")],.8));
    rotate(independent->pose(),1,{1,0,0},HalfPi);rotate(independent->pose(),2,{0,1,0},std::numbers::pi/3);
    auto other=meshes(*independent);for(auto *part:other)part->setMorphWeight(morph(*part,"keep"),.33);
    const auto corrected=parts[0]->deformedVertices(),ordinary=other[0]->deformedVertices();CHECK(corrected.size()==ordinary.size());
    double maximum=0;for(std::size_t i=0;i<corrected.size();++i)
        maximum=std::max(maximum,std::hypot(corrected[i].position.x-ordinary[i].position.x,
            corrected[i].position.y-ordinary[i].position.y,corrected[i].position.z-ordinary[i].position.z));
    CHECK(near(maximum,.2,1e-8));
    rotate(actor->pose(),1,{1,0,0},HalfPi/2);rotate(actor->pose(),2,{0,1,0},std::numbers::pi/6);
    CHECK(near(evaluateObject3DPoseCorrectives(rig,actor->pose()).blendShapes[1],.2));
    rotate(actor->pose(),1,{1,0,0},HalfPi);rotate(actor->pose(),2,{0,1,0},std::numbers::pi/3);
    auto signedPose=actor->pose().locals()[1];signedPose.rotation={-signedPose.rotation.x,-signedPose.rotation.y,-signedPose.rotation.z,-signedPose.rotation.w};
    actor->pose().setLocal(1,signedPose);CHECK(near(evaluateObject3DPoseCorrectives(rig,actor->pose()).blendShapes[0],1));
    rotate(actor->pose(),1,{1,0,0},-HalfPi);CHECK(near(evaluateObject3DPoseCorrectives(rig,actor->pose()).blendShapes[0],.6));
    std::vector<Transform> bind;for(const auto &bone:object.get().skeleton()->bones())bind.push_back(bone.bindTransform);
    bind[1].position.x+=.5;bind[1].rotation=multiply(bind[1].rotation,Quaternion::fromAxisAngle({0,0,1},.2));actor->pose().setDeformationBind(bind);
    CHECK(near(evaluateObject3DPoseCorrectives(rig,actor->pose()).blendShapes[0],.6));
    Pose foreign(std::make_shared<Skeleton>(std::vector<Bone>(object.get().skeleton()->bones().begin(),object.get().skeleton()->bones().end())));
    rejects([&]{(void)evaluateObject3DPoseCorrectives(rig,foreign);});
    const auto spoof=engine.reference().createRigSession(object,std::make_unique<SpoofedDriver>());
    const auto prior=actor->pose().locals()[1];rejects([&]{applyObject3DPoseCorrectives(spoof,*actor);});CHECK(actor->pose().locals()[1]==prior);
}
void component()
{
    Object3DEngine engine;const auto object=engine.createObject(data());const auto rig=createObject3DPoseCorrectiveRig(object,program());
    auto actor=object.get().instantiate();auto parts=meshes(*actor);auto start=actor->pose().locals()[1],end=start;
    end.rotation=multiply(start.rotation,Quaternion::fromAxisAngle({1,0,0},HalfPi));
    auto clip=std::make_shared<AnimationClip>(object.get().skeleton(),1,std::vector<BoneTrack>{{1,{{0,start},{1,end}}}});
    actor->animation().play(clip,AnimationWrap::Clamp);CHECK(actor->beginPlay());
    auto &correctives=attachObject3DPoseCorrectives(*actor,rig);CHECK(!correctives.error());
    CHECK(actor->tick(.5));CHECK(near(parts[0]->morphWeights()[morph(*parts[0],"elbow_fix")],.5));
    CHECK(actor->tick(.5));CHECK(near(parts[0]->morphWeights()[morph(*parts[0],"elbow_fix")],1));
    const auto before=parts[0]->morphWeights()[morph(*parts[0],"elbow_fix")];CHECK(engine.reference().eraseRig(rig.id()));
    CHECK(actor->tick(.1)&&correctives.error());CHECK(parts[0]->morphWeights()[morph(*parts[0],"elbow_fix")]==before);
    rejects([&]{correctives.update();});
}
void codec()
{
    const auto p=program();auto bytes=exportObject3DPoseCorrectiveProgram(p);const auto restored=importObject3DPoseCorrectiveProgram(bytes);
    CHECK(restored.samples.size()==4&&restored.samples[3].conditions.size()==2&&restored.samples[0].morph=="elbow_fix");
    auto damaged=bytes;damaged.back()^=std::byte{1};rejects([&]{(void)importObject3DPoseCorrectiveProgram(damaged);});
    rejects([&]{(void)importObject3DPoseCorrectiveProgram(std::span(bytes).first(bytes.size()-1));});
    rejects([&]{(void)exportObject3DPoseCorrectiveProgram(p,24);});
    auto bad=p;bad.samples[0].conditions[0].radiusRadians=0;rejects([&]{validateObject3DPoseCorrectiveProgram(bad);});
    bad=p;bad.samples[0].conditions[0].rotation={0,0,0,0};rejects([&]{validateObject3DPoseCorrectiveProgram(bad);});
    bad=p;bad.samples[0].conditions.push_back(bad.samples[0].conditions[0]);rejects([&]{validateObject3DPoseCorrectiveProgram(bad);});
    bad=p;bad.samples[0].maximumWeight=std::numeric_limits<double>::quiet_NaN();rejects([&]{validateObject3DPoseCorrectiveProgram(bad);});
    Object3DEngine engine;auto noMorph=data();noMorph.meshes[0].topology->morphTargets.clear();noMorph.meshes[1].topology->morphTargets.clear();
    const auto object=engine.createObject(noMorph);const auto before=engine.reference().rigCount();
    rejects([&]{(void)createObject3DPoseCorrectiveRig(object,p);});CHECK(engine.reference().rigCount()==before);
}
void rebind()
{
    Object3DEngine engine;const auto object=engine.createObject(data());const auto rig=createObject3DPoseCorrectiveRig(object,program());
    auto renamed=object.get().data();renamed.bones[1].name="edited_elbow";renamed.bones[2].name="edited_wrist";
    renamed.bones[1].bindTransform.rotation=Quaternion::fromAxisAngle({0,0,1},.6);
    const auto edited=engine.createObject(std::move(renamed));const std::array<BoneIndex,2> mapping{1,2};
    const auto rebound=rig.get().cloneFor(edited,mapping);CHECK(rebound.get().sourceBytes()==rig.get().sourceBytes());
    auto actor=edited.get().instantiate();rotate(actor->pose(),1,{1,0,0},HalfPi);applyObject3DPoseCorrectives(rebound,*actor);
    CHECK(near(meshes(*actor)[0]->morphWeights()[morph(*meshes(*actor)[0],"elbow_fix")],1));
    auto missing=edited.get().data();for(auto &mesh:missing.meshes)mesh.topology->morphTargets.clear();
    const auto incompatible=engine.createObject(std::move(missing));const auto count=engine.reference().rigCount();
    rejects([&]{(void)rig.get().cloneFor(incompatible,mapping);});CHECK(engine.reference().rigCount()==count);
#if IIACTOROBJECT_HAS_NSCHARACTER
    Object3DTemplateDefinition definition;definition.base=object;
    NSCharacter::CharacterTemplate character(engine.createTemplate(std::move(definition)));
    NSCharacter::CharacterAsset view(object);const auto wrapped=character.createPoseCorrectiveRig(view,program());
    auto wrappedActor=object.get().instantiate();rotate(wrappedActor->pose(),1,{1,0,0},HalfPi);
    applyObject3DPoseCorrectives(wrapped,*wrappedActor);CHECK(near(meshes(*wrappedActor)[0]->morphWeights()[morph(*meshes(*wrappedActor)[0],"elbow_fix")],1));
    Object3DEngine foreignEngine;NSCharacter::CharacterAsset foreign(foreignEngine.createObject(data()));
    rejects([&]{(void)character.createPoseCorrectiveRig(foreign,program());});
#endif
}
void project()
{
    Object3DEngine engine;const auto original=engine.createObject(data());auto editor=engine.editObject(original);
    editor.get().moveControlPoints(0,std::array{TopologyPointMove{0,{.001,0,0}}});
    const auto rig=createObject3DPoseCorrectiveRig(editor.get().object(),program());
    const auto second=createObject3DPoseCorrectiveRig(editor.get().object(),program());
    const Object3DPoseProgram poseProgram{{"elbow"},{{"turn",{Quaternion::fromAxisAngle({0,1,0},.3)}}}};
    const auto poseRig=createObject3DPoseRig(editor.get().object(),poseProgram);
    const auto bytes=exportObject3DProject(editor,std::array{rig,second,poseRig});CHECK(bytes[8]==26);
    Object3DEngine target;const auto restored=importObject3DProject(target.reference(),bytes);CHECK(restored.rigs.size()==3);
    CHECK(restored.rigs[0].get().sourceBytes()==rig.get().sourceBytes());auto actor=restored.rigs[0].get().object().get().instantiate();
    rotate(actor->pose(),1,{1,0,0},HalfPi);applyObject3DPoseCorrectives(restored.rigs[0],*actor);
    CHECK(near(meshes(*actor)[0]->morphWeights()[morph(*meshes(*actor)[0],"elbow_fix")],1));
    auto another=restored.rigs[1].get().object().get().instantiate();rotate(another->pose(),1,{1,0,0},-HalfPi);
    applyObject3DPoseCorrectives(restored.rigs[1],*another);
    CHECK(near(meshes(*another)[0]->morphWeights()[morph(*meshes(*another)[0],"elbow_fix")],.6));
    CHECK(near(meshes(*actor)[0]->morphWeights()[morph(*meshes(*actor)[0],"elbow_fix")],1));
    const auto expectedPose=poseRig.get().evaluateNamed(std::array{NamedRigControl{"turn",.5}});
    const auto restoredPose=restored.rigs[2].get().evaluateNamed(std::array{NamedRigControl{"turn",.5}});
    const auto a=expectedPose.joints[1].rotation,b=restoredPose.joints[1].rotation;
    CHECK(near(std::abs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w),1));
    CHECK(restored.editor.get().undo()&&restored.editor.get().redo());
    auto corrupted=bytes;corrupted.back()^=1;const auto count=target.objectCount(),rigs=target.reference().rigCount();
    rejects([&]{(void)importObject3DProject(target.reference(),corrupted);});
    CHECK(target.objectCount()==count&&target.reference().rigCount()==rigs);
    corrupted=bytes;const std::array<std::uint8_t,8> signature{'i','i','A','O','P','C','R',0};
    const auto embedded=std::search(corrupted.begin()+24,corrupted.end(),signature.begin(),signature.end());
    CHECK(embedded!=corrupted.end()&&static_cast<std::size_t>(corrupted.end()-embedded)>24);
    CHECK(std::search(embedded+8,corrupted.end(),signature.begin(),signature.end())==corrupted.end());*(embedded+24)^=1;
    const auto checksum=crc(std::span<const std::uint8_t>(corrupted).subspan(24));
    for(unsigned i=0;i<4;++i)corrupted[20+i]=static_cast<std::uint8_t>(checksum>>(i*8));
    const auto editors=target.reference().editorCount();rejects([&]{(void)importObject3DProject(target.reference(),corrupted);});
    CHECK(target.objectCount()==count&&target.reference().rigCount()==rigs&&target.reference().editorCount()==editors);
}
void formats(const std::filesystem::path &directory)
{
    Object3DEngine engine;const auto object=engine.createObject(data());const auto editor=engine.editObject(object);
    const auto rig=createObject3DPoseCorrectiveRig(object,program());
    const auto standalone=exportObject3DPoseCorrectiveProgram(program());const auto projectBytes=exportObject3DProject(editor,std::array{rig});
    std::filesystem::create_directories(directory);
    const auto save=[](const auto &path,const auto &bytes)
    {std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));CHECK(file.good());};
    save(directory/"corrective.iipc",standalone);save(directory/"corrective.ii3dproject",projectBytes);
    save(directory/"corrective.fbx",exportObject3DFbx(object.get()));
    save(directory/"corrective.glb",exportObject3DGlb(object.get()));
}
#if IIACTOROBJECT_HAS_NSCHARACTER
void human(const std::filesystem::path &directory)
{
    Object3DEngine engine;NSCharacter::MakeHumanTemplateOptions load;load.semanticControls=false;
    const auto character=NSCharacter::loadMakeHumanTemplate(engine.reference(),directory,load);
    const auto generated=character.generate();auto humanData=generated->data();
    CHECK(humanData.bones.size()>=150&&humanData.meshes[0].topology->points.size()>=19'000);
    const auto elbow=generated->skeleton()->findBone("lowerarm01.L");CHECK(elbow!=NoBone);
    auto &topology=*humanData.meshes[0].topology;const auto &layout=topology.layout->definition();
    std::uint32_t sourcePoint=NoTopologyIndex;std::size_t render=0;
    for(std::size_t i=0;i<layout.renderToLayout.size();++i)
    {
        const auto point=layout.layouts.at(layout.renderToLayout[i]).position;
        if(std::any_of(topology.points[point].weights.begin(),topology.points[point].weights.end(),
            [&](const auto &weight){return weight.bone==elbow&&weight.weight>.25;}))
        {sourcePoint=point;render=i;break;}
    }
    CHECK(sourcePoint!=NoTopologyIndex);topology.morphTargets.push_back({"elbow_fix_fixture",{{sourcePoint,{0,0,.005}}}});
    const auto object=engine.createObject(std::move(humanData));NSCharacter::CharacterAsset asset(object);
    Object3DPoseCorrectiveProgram authored{{{"elbow_fix_fixture",{{"lowerarm01.L",Quaternion::fromAxisAngle({1,0,0},.8),.8}},1}}};
    const auto rig=character.createPoseCorrectiveRig(asset,authored);auto corrected=asset.instantiate(),ordinary=asset.instantiate();
    rotate(corrected->pose(),elbow,{1,0,0},.8);rotate(ordinary->pose(),elbow,{1,0,0},.8);
    applyObject3DPoseCorrectives(rig,*corrected);
    const auto a=meshes(*corrected)[0]->deformedVertices()[render].position,b=meshes(*ordinary)[0]->deformedVertices()[render].position;
    CHECK(std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)>1e-6);
}
#endif
int main(int argc,char **argv)
{
    try{if(argc==3&&std::string_view(argv[1])=="formats"){formats(argv[2]);return 0;}
#if IIACTOROBJECT_HAS_NSCHARACTER
        if(argc==3&&std::string_view(argv[1])=="human"){human(argv[2]);return 0;}
#endif
        if(argc!=2)return 2;const std::string_view mode=argv[1];if(mode=="evaluate")evaluate();else if(mode=="component")component();
        else if(mode=="codec")codec();else if(mode=="rebind")rebind();else if(mode=="project")project();else return 2;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}return 0;
}
