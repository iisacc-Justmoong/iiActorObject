#include <iiActorObject/Object3DRig.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DTemplateMorphs.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <type_traits>

using namespace iiActorObject;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)
template<class F> void rejects(F f)
{
    bool caught = false;
    try { f(); } catch (const std::exception &) { caught = true; }
    CHECK(caught);
}
struct TestDriver final : Object3DRigDriver
{
    Object3DRigDefinition info;
    bool badOutput = false;
    TestDriver()
    {
        info.joints = {"root", "jaw"};
        info.rawControls = {"smile"};
        info.guiControls = {"smile"};
        info.blendShapes = {"Smile"};
        info.animatedMaps = {"wrinkle"};
        info.rawControlDefaults = {.1};
        info.guiControlDefaults = {.2};
        info.lodCount = 2;
    }
    const Object3DRigDefinition &definition() const noexcept override { return info; }
    std::unique_ptr<Object3DRigDriver> clone() const override
    { return std::make_unique<TestDriver>(*this); }
    Object3DRigOutputs evaluate(std::span<const double> controls, RigControlSpace, std::size_t lod) override
    {
        Object3DRigOutputs out;
        out.joints.resize(badOutput ? 1 : 2);
        if (!badOutput) out.joints[1].translation.y = .02 * controls[0];
        out.blendShapes = {lod == 0 ? controls[0] : 0};
        out.animatedMaps = {controls[0] * controls[0]};
        return out;
    }
};
Object3DData templateData()
{
    Object3DData d;
    d.name = "RigTest";
    d.bones = {{"root", NoBone, {}}, {"jaw", 0, {{0, 1, 0}, {}, {1, 1, 1}}}};
    Object3DMesh mesh;
    mesh.name = "Face";
    mesh.geometry.vertices = {{{0, 1, 0}, {0, 0, 1}, {{1, 1}}},
                              {{1, 1, 0}, {0, 0, 1}, {{1, 1}}},
                              {{0, 2, 0}, {0, 0, 1}, {{1, 1}}}};
    mesh.geometry.triangles = {0, 1, 2};
    mesh.geometry.morphTargets = {{"Smile", {{0, {0, .2, 0}, {}}}}};
    d.meshes.push_back(mesh);
    return d;
}
void ownership()
{
    static_assert(!std::is_copy_constructible_v<Object3DRigSession>);
    Object3DRigHandle retained;
    {
        Object3DEngine engine;
        auto object = engine.createObject(templateData());
        retained = engine.createRigSession(object, std::make_unique<TestDriver>());
        object = {};
        engine.collectGarbage();
        CHECK(engine.objectCount() == 1 && engine.rigCount() == 1);
        CHECK(retained.valid() && retained.get().object().valid());
    }
    CHECK(!retained.valid());
    rejects([&] { (void)retained.get(); });
    Object3DEngine engine;
    {
        auto object = engine.createObject(templateData());
        auto rig = engine.createRigSession(object, std::make_unique<TestDriver>());
    }
    CHECK(engine.collectGarbage() == 2);
    CHECK(engine.rigCount() == 0 && engine.objectCount() == 0);
}
void evaluation()
{
    Object3DEngine engine;
    auto object = engine.createObject(templateData());
    auto rig = engine.createRigSession(object, std::make_unique<TestDriver>());
    const auto evaluated = rig.get().evaluate(std::array{.5});
    CHECK(std::abs(evaluated.joints[1].position.y - 1.01) < 1e-12);
    CHECK(evaluated.blendShapes[0] == .5 && evaluated.animatedMaps[0] == .25);
    auto actor = object.get().instantiate();
    rig.get().applyTo(*actor, evaluated);
    SkeletalMeshComponent *mesh = nullptr;
    for (auto *component : actor->components())
        if (auto *part = dynamic_cast<SkeletalMeshComponent *>(component)) mesh = part;
    CHECK(mesh && std::abs(mesh->deformedVertices()[0].position.y - 1.11) < 1e-12);
    const auto lower = rig.get().evaluate(std::array{.5}, RigControlSpace::Raw, 1);
    CHECK(lower.blendShapes[0] == 0);
    const auto gui = rig.get().evaluate(std::array{.75}, RigControlSpace::Gui);
    CHECK(gui.blendShapes[0] == .75);
    CHECK(rig.get().evaluateNamed({}).blendShapes[0] == .1);
    CHECK(rig.get().evaluateNamed(std::array{NamedRigControl{"smile", .25}}).blendShapes[0] == .25);
    rejects([&] { (void)rig.get().evaluateNamed(std::array{NamedRigControl{"missing", .25}}); });
}
void morphChannels()
{
    Object3DEngine engine;auto data=templateData();
    data.meshes[0].geometry.morphTargets.push_back({"Wink",{{1,{0,0,.1},{}}}});
    auto mouth=data.meshes[0];mouth.name="Mouth";
    mouth.geometry.morphTargets={{"Tongue",{{0,{0,0,.3},{}}}},{"Smile",{{2,{0,.1,0},{}}}}};
    data.meshes.push_back(std::move(mouth));
    const auto object=engine.createObject(data);const auto rig=createObject3DMorphChannelRig(object);
    CHECK(rig.valid()&&engine.reference().owns(rig.get().object())&&rig.get().object()==object);
    CHECK(rig.get().definition().rawControls==std::vector<std::string>({"Smile","Wink","Tongue"}));
    CHECK(rig.get().definition().guiControls==rig.get().definition().rawControls);
    const auto metadata=inspectObject3DTemplateMorphRig(rig);
    CHECK(!metadata.templateProgram.valid()&&!metadata.program.baselineInputs&&!metadata.program.bakeOptions);
    const auto values=std::array{NamedRigControl{"Smile",.25},NamedRigControl{"Tongue",.75}};
    const auto result=rig.get().evaluateNamed(values);CHECK(result.blendShapes==std::vector<double>({.25,0,.75}));
    auto actor=object.get().instantiate();auto jaw=actor->pose().locals()[1];jaw.position.x=.4;actor->pose().setLocal(1,jaw);
    rig.get().applyTo(*actor,result);CHECK(actor->pose().locals()[1]==jaw);
    std::vector<SkeletalMeshComponent *> meshes;
    for(auto *component:actor->components())if(auto *mesh=dynamic_cast<SkeletalMeshComponent *>(component))meshes.push_back(mesh);
    CHECK(meshes.size()==2&&meshes[0]->morphWeights()[0]==.25&&meshes[0]->morphWeights()[1]==0);
    CHECK(meshes[1]->morphWeights()[0]==.75&&meshes[1]->morphWeights()[1]==.25);
    CHECK(rig.get().evaluateNamed(values,RigControlSpace::Gui).blendShapes==result.blendShapes);
    rejects([&]{(void)rig.get().evaluateNamed(std::array{NamedRigControl{"Tongue",1.1}});});
    rejects([&]{(void)rig.get().evaluateNamed(std::array{NamedRigControl{"unknown",.5}});});
    auto missing=data;missing.meshes[1].geometry.morphTargets.erase(missing.meshes[1].geometry.morphTargets.begin());
    const auto altered=engine.createObject(std::move(missing));const auto rigs=engine.rigCount();
    rejects([&]{(void)rig.get().cloneFor(altered);});CHECK(engine.rigCount()==rigs);
    const auto editor=engine.editObject(object);const auto bytes=exportObject3DProject(editor,std::array{rig});
    Object3DEngine restoredEngine;const auto restored=importObject3DProject(restoredEngine.reference(),bytes);
    CHECK(restored.rigs.size()==1&&restored.rigs[0].get().evaluateNamed(values).blendShapes==result.blendShapes);
    CHECK(!inspectObject3DTemplateMorphRig(restored.rigs[0]).templateProgram.valid());
    data.meshes.resize(1);data.meshes[0].geometry.morphTargets.clear();const auto empty=engine.createObject(std::move(data));
    const auto count=engine.rigCount();rejects([&]{(void)createObject3DMorphChannelRig(empty);});CHECK(engine.rigCount()==count);
}
void clone()
{
    Object3DEngine engine;
    auto object = engine.createObject(templateData());
    auto rig = engine.createRigSession(object, std::make_unique<TestDriver>());
    auto editor = engine.editObject(object);
    editor.get().moveJoint(1, {0, 2, 0});
    auto rebound = rig.get().cloneFor(editor.get().object());
    CHECK(rebound.get().object() == editor.get().object());
    CHECK(std::abs(rebound.get().evaluate(std::array{.5}).joints[1].position.y - 2.01) < 1e-12);
    CHECK(std::abs(rig.get().evaluate(std::array{.5}).joints[1].position.y - 1.01) < 1e-12);
    Object3DEngine other;
    auto foreign = other.createObject(templateData());
    rejects([&] { (void)rig.get().cloneFor(foreign); });
    CHECK(engine.eraseRig(rebound.id()) && !rebound.valid());
    editor.get().renameJoint(1, "chin");
    rejects([&] { (void)rig.get().cloneFor(editor.get().object()); });
    auto renamed = rig.get().cloneFor(editor.get().object(), std::array<BoneIndex,2>{0,1});
    CHECK(renamed.get().evaluate(std::array{.5}).joints.size() == 2);
    auto swapEditor=engine.editObject(object);CHECK(swapEditor.get().renameJoints(std::array{JointRename{0,"jaw"},JointRename{1,"root"}}));
    const auto stable=rig.get().cloneFor(swapEditor.get().object(),rig.get().jointMapping());
    CHECK(stable.get().jointMapping()[0]==0&&stable.get().jointMapping()[1]==1);
    CHECK(std::abs(stable.get().evaluate(std::array{.5}).joints[1].position.y-1.01)<1e-12);
    const auto byName=rig.get().cloneFor(swapEditor.get().object());
    CHECK(byName.get().jointMapping()[0]==1&&byName.get().jointMapping()[1]==0);
}
void validation()
{
    Object3DEngine engine({16, 16, 1});
    auto object = engine.createObject(templateData());
    auto driver = std::make_unique<TestDriver>();
    driver->info.joints[1] = "missing";
    rejects([&] { (void)engine.createRigSession(object, std::move(driver)); });
    CHECK(engine.rigCount() == 0);
    auto rig = engine.createRigSession(object, std::make_unique<TestDriver>());
    rejects([&] { (void)engine.createRigSession(object, std::make_unique<TestDriver>()); });
    rejects([&] { (void)rig.get().evaluate(std::array{.5, .2}); });
    rejects([&] { (void)rig.get().evaluate(std::array{std::numeric_limits<double>::quiet_NaN()}); });
    rejects([&] { (void)rig.get().evaluate(std::array{.5}, RigControlSpace::Raw, 2); });
    CHECK(engine.eraseObject(object.id()));
    rejects([&] { (void)rig.get().evaluate(std::array{.5}); });
    CHECK(engine.eraseRig(rig.id()));
    auto next = engine.createObject(templateData());
    auto broken = std::make_unique<TestDriver>();
    broken->badOutput = true;
    auto invalid = engine.createRigSession(next, std::move(broken));
    rejects([&] { (void)invalid.get().evaluate(std::array{.5}); });
}
int main(int argc, char **argv)
{
    try
    {
        if (argc != 2) return 2;
        const std::string name = argv[1];
        if (name == "ownership") ownership();
        else if (name == "evaluation") {evaluation();morphChannels();}
        else if (name == "clone") clone();
        else if (name == "validation") validation();
        else return 2;
        std::cout << name << " passed\n";
    }
    catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
