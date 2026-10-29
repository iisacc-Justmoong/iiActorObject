#include <iiActorObject/Object3DEditorRig.h>
#include <iiActorObject/Object3DPoseRig.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DTemplateMorphs.h>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace iiActorObject;
#define CHECK(x) do { if(!(x))throw std::runtime_error(#x); } while(false)
template<class F>void rejects(F action){bool failed=false;try{action();}catch(const std::exception&){failed=true;}CHECK(failed);}
enum class CloneBehavior { Normal, Fail, ChangeControls };
struct Driver final : Object3DRigDriver
{
    Object3DRigDefinition info;
    CloneBehavior behavior=CloneBehavior::Normal;
    explicit Driver(CloneBehavior value=CloneBehavior::Normal):behavior(value)
    {
        info.joints={"root","jaw"};info.rawControls={"smile"};info.guiControls={"smile"};
        info.rawControlDefaults={.1};info.guiControlDefaults={.2};info.blendShapes={"Smile"};
        info.animatedMaps={"wrinkle"};info.lodCount=2;
    }
    const Object3DRigDefinition &definition()const noexcept override{return info;}
    std::unique_ptr<Object3DRigDriver> clone()const override{return std::make_unique<Driver>(*this);}
    std::unique_ptr<Object3DRigDriver> cloneFor(const RigSourceSnapshot &)const override
    {
        if(behavior==CloneBehavior::Fail)throw std::invalid_argument("target rejected");
        auto next=std::make_unique<Driver>(*this);
        if(behavior==CloneBehavior::ChangeControls)next->info.rawControls[0]="changed";
        return next;
    }
    Object3DRigOutputs evaluate(std::span<const double> controls,RigControlSpace,std::size_t lod)override
    {
        Object3DRigOutputs value;value.joints.resize(2);value.joints[1].translation.y=.02*controls[0];
        value.blendShapes={lod==0?controls[0]:0};value.animatedMaps={controls[0]*controls[0]};return value;
    }
};
Object3DData data(bool semantic=true)
{
    Object3DData result;result.name="RigFollow";
    result.bones={{"root",NoBone,{}},{"jaw",0,{{0,1,0},{},{1,1,1}}}};
    if(semantic)result.semantics={{"Root",0},{"Jaw",1}};
    Object3DMesh mesh;mesh.name="Face";
    mesh.geometry.vertices={{{0,1,0},{0,0,1},{{1,1}}},{{1,1,0},{0,0,1},{{1,1}}},{{0,2,0},{0,0,1},{{1,1}}}};
    mesh.geometry.triangles={0,1,2};mesh.geometry.morphTargets={{"Smile",{{0,{0,.2,0},{}}}}};
    result.meshes.push_back(std::move(mesh));return result;
}
bool near(double a,double b){return std::abs(a-b)<1e-10;}
void follow()
{
    Object3DEngine engine;const auto original=engine.createObject(data());const auto editor=engine.editObject(original);
    const auto a=engine.createRigSession(original,std::make_unique<Driver>()),b=engine.createRigSession(original,std::make_unique<Driver>());
    Object3DEditorRigSet rigSet(editor,std::array{a,b});CHECK(!rigSet.synchronize());
    const auto first=rigSet.rig(0);CHECK(first.id()==a.id());
    CHECK(near(rigSet.evaluate(0,std::array{.5}).joints[1].position.y,1.01));
    editor.get().setPointSelection("Face",std::array{MeshPointMask{0,{1,0,0}}});
    CHECK(!rigSet.synchronize()&&rigSet.rig(0).id()==first.id());
    editor.get().moveJoint(1,{0,2,0});CHECK(rigSet.synchronize());
    const auto rebound=rigSet.rig(0);CHECK(rebound.id()!=a.id()&&rigSet.rig(1).id()!=b.id());
    CHECK(rebound.get().object()==editor.get().object()&&near(rigSet.evaluateNamed(0,std::array{NamedRigControl{"smile",.5}}).joints[1].position.y,2.01));
    CHECK(near(a.get().evaluate(std::array{.5}).joints[1].position.y,1.01));
    CHECK(editor.get().renameJoints(std::array{JointRename{0,"jaw"},JointRename{1,"root"}}));
    const auto swapped=rigSet.evaluate(0,std::array{.5});const auto mapped=rigSet.rig(0).get().jointMapping();
    CHECK(mapped[0]==0&&mapped[1]==1&&near(swapped.joints[1].position.y,2.01));
    CHECK(editor.get().undo());CHECK(rigSet.synchronize()&&rigSet.rig(0).get().object()==editor.get().object());
    CHECK(editor.get().redo());CHECK(rigSet.synchronize()&&rigSet.rig(0).get().jointMapping()[1]==1);
    const auto actor=editor.get().object().get().instantiate();const auto evaluation=rigSet.evaluate(0,std::array{.5});
    rigSet.rig(0).get().applyTo(*actor,evaluation);
    CHECK(near(actor->pose().locals()[1].position.y,2.01));
}
void identity()
{
    Object3DEngine engine;const auto original=engine.createObject(data());const auto editor=engine.editObject(original);
    const auto rig=engine.createRigSession(original,std::make_unique<Driver>());Object3DEditorRigSet view(editor,std::array{rig});
    auto changed=original.get().data();changed.semantics[1].bone=0;editor.get().replace(std::move(changed));
    const auto count=engine.rigCount();rejects([&]{(void)view.rig(0);});CHECK(engine.rigCount()==count);
    CHECK(editor.get().undo()&&!view.synchronize()&&view.rig(0).id()==rig.id());
    auto plain=engine.createObject(data(false));auto edit=engine.editObject(plain);auto source=engine.createRigSession(plain,std::make_unique<Driver>());
    Object3DEditorRigSet guarded(edit,std::array{source});Object3DEditorRigOptions options;options.jointIdentity=EditedRigJointIdentity::TrustStableIndices;
    Object3DEditorRigSet trusted(edit,std::array{source},options);edit.get().renameJoint(1,"renamed");
    rejects([&]{(void)guarded.rig(0);});CHECK(trusted.rig(0).get().jointMapping()[1]==1);
    CHECK(near(trusted.evaluate(0,std::array{.5}).joints[1].position.y,1.01));
    auto foreign=engine.editObject(original);foreign.get().moveJoint(1,{0,3,0});
    rejects([&]{Object3DEditorRigSet invalid(foreign,std::array{rig});});
    rejects([&]{Object3DEditorRigSet invalid(editor,std::array{rig,rig});});
    rejects([&]{Object3DEditorRigSet invalid(editor,std::span<const Object3DRigHandle>{});});
    Object3DEngine other;const auto otherObject=other.createObject(data());
    const auto otherRig=other.createRigSession(otherObject,std::make_unique<Driver>());
    rejects([&]{Object3DEditorRigSet invalid(editor,std::array{otherRig});});
    Object3DEditorRigOptions invalidOptions;invalidOptions.jointIdentity=static_cast<EditedRigJointIdentity>(255);
    rejects([&]{Object3DEditorRigSet invalid(editor,std::array{rig},invalidOptions);});
    rejects([&]{(void)view.rig(1);});
    auto additional=original.get().data();additional.bones.push_back({"extra",0,{}});editor.get().replace(std::move(additional));
    rejects([&]{(void)view.synchronize();});CHECK(editor.get().undo()&&!view.synchronize());
}
void atomic()
{
    for(const auto behavior:{CloneBehavior::Fail,CloneBehavior::ChangeControls})
    {
        Object3DEngine engine;const auto object=engine.createObject(data());const auto editor=engine.editObject(object);
        const auto a=engine.createRigSession(object,std::make_unique<Driver>()),b=engine.createRigSession(object,std::make_unique<Driver>(behavior));
        Object3DEditorRigSet set(editor,std::array{a,b});editor.get().moveJoint(1,{0,2,0});
        const auto before=engine.rigCount();rejects([&]{(void)set.synchronize();});
        CHECK(engine.rigCount()==before&&a.valid()&&b.valid()&&near(a.get().evaluate(std::array{.5}).joints[1].position.y,1.01));
        CHECK(editor.get().undo()&&!set.synchronize()&&set.rig(0).id()==a.id()&&set.rig(1).id()==b.id());
    }
    Object3DEngine limited({16,16,3});const auto object=limited.createObject(data());const auto editor=limited.editObject(object);
    const auto a=limited.createRigSession(object,std::make_unique<Driver>()),b=limited.createRigSession(object,std::make_unique<Driver>());
    Object3DEditorRigSet set(editor,std::array{a,b});editor.get().moveJoint(1,{0,2,0});
    rejects([&]{(void)set.synchronize();});CHECK(limited.rigCount()==2&&a.valid()&&b.valid());
}
void realPrograms()
{
    Object3DEngine engine;const auto object=engine.createObject(data());const auto editor=engine.editObject(object);
    Object3DPoseProgram program;program.joints={"root","jaw"};
    program.units={{"Mouth",{{},Quaternion::fromAxisAngle({0,0,1},.3)}}};
    const auto pose=createObject3DPoseRig(object,program),morph=createObject3DMorphChannelRig(object);
    Object3DEditorRigSet set(editor,std::array{pose,morph});editor.get().moveJoint(1,{0,2,0});
    const auto face=set.evaluateNamed(0,std::array{NamedRigControl{"Mouth",1}});
    const auto shape=set.evaluateNamed(1,std::array{NamedRigControl{"Smile",.5}});
    CHECK(face.object==editor.get().object()&&shape.object==face.object&&shape.blendShapes[0]==.5);
    auto actor=editor.get().object().get().instantiate();set.rig(0).get().applyTo(*actor,face);set.rig(1).get().applyTo(*actor,shape);
    CHECK(std::abs(actor->pose().locals()[1].rotation.z)>.1&&near(actor->findComponent<SkeletalMeshComponent>()->morphWeights()[0],.5));
    const auto bytes=exportObject3DProject(editor,set.rigs());Object3DEngine destination;
    const auto restored=importObject3DProject(destination.reference(),bytes);
    Object3DEditorRigSet imported(restored.editor,restored.rigs);
    CHECK(imported.evaluateNamed(1,std::array{NamedRigControl{"Smile",.5}}).blendShapes[0]==.5);
    CHECK(imported.evaluateNamed(0,std::array{NamedRigControl{"Mouth",1}}).object==restored.editor.get().object());
    const auto a=set.rig(0),b=set.rig(1);auto missing=editor.get().object().get().data();
    missing.meshes[0].geometry.morphTargets.clear();editor.get().replace(std::move(missing));
    const auto count=engine.rigCount();rejects([&]{(void)set.synchronize();});
    CHECK(engine.rigCount()==count&&a.valid()&&b.valid());
    CHECK(editor.get().undo()&&!set.synchronize()&&set.rig(0).id()==a.id()&&set.rig(1).id()==b.id());
}
void lifetime()
{
    std::unique_ptr<Object3DEditorRigSet> set;Object3DRigHandle saved;
    {
        Object3DEngine engine;const auto object=engine.createObject(data());const auto editor=engine.editObject(object);
        saved=engine.createRigSession(object,std::make_unique<Driver>());
        set=std::make_unique<Object3DEditorRigSet>(editor,std::array{saved});
        CHECK(set->rig(0).valid());
    }
    CHECK(!saved.valid());rejects([&]{(void)set->synchronize();});
}
int main(int argc,char **argv)
{
    try{if(argc!=2)return 2;const std::string_view mode=argv[1];if(mode=="follow")follow();else if(mode=="identity")identity();
        else if(mode=="atomic")atomic();else if(mode=="real_programs")realPrograms();else if(mode=="lifetime")lifetime();else return 2;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
