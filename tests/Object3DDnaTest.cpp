#include <iiActorObject/Object3DDna.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DTemplateMorphs.h>
#include <iiActorObject/Object3DProgressiveMorph.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/DnaCharacter.h>
#endif
#include <array>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while (false)
template<class F> void rejects(F f)
{
    bool caught = false;
    try { f(); } catch (const std::exception &) { caught = true; }
    CHECK(caught);
}
std::vector<std::byte> load(const std::filesystem::path &path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Fixture missing");
    std::vector<char> raw((std::istreambuf_iterator<char>(f)), {});
    const auto view = std::as_bytes(std::span(raw));
    return {view.begin(), view.end()};
}
bool near(double a, double b) { return std::abs(a-b) < 2e-6; }
void save(const std::filesystem::path &path,std::span<const std::byte> bytes)
{
    std::ofstream file(path,std::ios::binary);
    file.write(reinterpret_cast<const char *>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    if (!file) throw std::runtime_error("DNA output failed");
}
void imported(const std::filesystem::path &dir)
{
    CHECK(object3DDnaAvailable());
    Object3DEngine engine;
    const auto bytes = load(dir/"independent.dna");
    auto result = importObject3DDna(engine.reference(), bytes);
    CHECK(result.object.valid() && result.rig.valid());
    const auto &d = result.object.get().data();
    CHECK(d.bones.size() == 2 && d.bones[1].name == "jaw");
    CHECK(near(d.bones[1].bindTransform.position.y, 1));
    CHECK(d.meshes.size() == 1 && d.meshes[0].geometry.vertices.size() == 4);
    CHECK(d.meshes[0].geometry.triangles.size() == 6);
    CHECK(d.meshes[0].textureCoordinates.size() == 4);
    CHECK(d.meshes[0].geometry.morphTargets[0].name == "Smile");
    CHECK(result.rig.get().definition().rawControls == std::vector<std::string>{"smile"});
    CHECK(result.rig.get().definition().lodCount == 2);
    CHECK(result.rig.get().sourceBytes() == bytes);
#if IIACTOROBJECT_HAS_NSCHARACTER
    std::shared_ptr<const NSCharacter::CharacterAsset> retained;
    Object3DRigHandle retainedRig;
    {
        Object3DEngine temporary;
        auto character = NSCharacter::importDna(temporary, bytes);
        retained = character.asset;
        retainedRig = character.rig;
        CHECK(&retained->data() == &retainedRig.get().object().get().data());
    }
    CHECK(!retained->valid() && !retainedRig.valid());
#endif
}
void controls(const std::filesystem::path &dir)
{
    Object3DEngine engine;
    auto result = importObject3DDna(engine.reference(), load(dir/"independent.dna"));
    auto &rig = result.rig.get();
    const auto value = rig.evaluate(std::array{.5});
    // PSD = .5 squared, then 2 cm and 30 degrees times PSD.
    CHECK(near(value.joints[1].position.y, 1.005));
    CHECK(near(value.joints[1].rotation.z, std::sin(27.5 * std::numbers::pi / 360)));
    CHECK(near(value.blendShapes[0], .5) && near(value.animatedMaps[0], .25));
    const auto gui = rig.evaluate(std::array{.5}, RigControlSpace::Gui);
    CHECK(near(gui.joints[1].position.y, value.joints[1].position.y));
    CHECK(near(gui.joints[1].rotation.z, value.joints[1].rotation.z));
    const auto lower = rig.evaluate(std::array{.5}, RigControlSpace::Raw, 1);
    CHECK(near(lower.joints[1].position.y, 1) && lower.blendShapes[0] == 0 && lower.animatedMaps[0] == 0);
    const auto reset = rig.evaluate(std::array{0.});
    CHECK(near(reset.joints[1].rotation.z, std::sin(20 * std::numbers::pi / 360)));
    auto actor = result.object.get().instantiate();
    rig.applyTo(*actor, value);
    bool deformed = false;
    for (auto *component : actor->components())
        if (auto *mesh = dynamic_cast<SkeletalMeshComponent *>(component))
        {
            CHECK(mesh->morphWeights()[0] == .5);
            CHECK(mesh->deformedVertices()[0].position.y > 1.09);
            deformed = true;
        }
    CHECK(deformed);
}
void progressiveProject(const std::filesystem::path &dir)
{
    Object3DEngine engine;const auto source=importObject3DDna(engine.reference(),load(dir/"independent.dna"));
    ProgressiveMorphProgram program{{{"SmileCurve",0,{{"Smile",.5}}}}};
    program.channels[0].sourceMeshes={source.object.get().data().meshes[0].name};
    const auto progressive=createObject3DProgressiveMorphRig(source.object,program);
    const auto editor=engine.editObject(source.object);
    const auto bytes=exportObject3DProject(editor,std::array{source.rig,progressive});CHECK(bytes.size()>24&&bytes[8]==31);
    Object3DEngine restored;const auto loaded=importObject3DProject(restored.reference(),bytes);
    CHECK(loaded.rigs.size()==2&&loaded.rigs[0].get().sourceBytes()==source.rig.get().sourceBytes());
    CHECK(inspectObject3DProgressiveMorphRig(loaded.rigs[1])==program);
    auto actor=loaded.rigs[0].get().object().get().instantiate();
    loaded.rigs[0].get().applyTo(*actor,loaded.rigs[0].get().evaluate(std::array{.5}));
    const auto before=actor->pose().locals()[1];
    loaded.rigs[1].get().applyTo(*actor,loaded.rigs[1].get().evaluateNamed(std::array{NamedRigControl{"SmileCurve",.25}}));
    CHECK(actor->pose().locals()[1]==before);
    const auto *mesh=actor->findComponent<SkeletalMeshComponent>();CHECK(mesh&&near(mesh->morphWeights()[0],.5));
}
void axes(const std::filesystem::path &dir)
{
    Object3DEngine engine;
    auto a = importObject3DDna(engine.reference(), load(dir/"independent.dna"));
    auto b = importObject3DDna(engine.reference(), load(dir/"axes.dna"));
    const auto x = a.rig.get().evaluate(std::array{.5});
    const auto y = b.rig.get().evaluate(std::array{.5});
    CHECK(near(x.joints[1].position.y, y.joints[1].position.y));
    CHECK(near(x.joints[1].rotation.z, y.joints[1].rotation.z));
    const auto &av = a.object.get().data().meshes[0].geometry.vertices;
    const auto &bv = b.object.get().data().meshes[0].geometry.vertices;
    for (std::size_t i=0; i<av.size(); ++i)
    {
        CHECK(near(av[i].position.y, bv[i].position.y));
        CHECK(near(av[i].normal.z, bv[i].normal.z));
    }
}
void limits(const std::filesystem::path &dir)
{
    Object3DEngine engine;
    const auto bytes = load(dir/"independent.dna");
    DnaImportOptions options;
    options.maximumVertices = 3;
    rejects([&] { (void)importObject3DDna(engine.reference(), bytes, options); });
    options = {};
    options.maximumMemory = 1024;
    rejects([&] { (void)importObject3DDna(engine.reference(), bytes, options); });
    rejects([&] { (void)importObject3DDna(engine.reference(), std::array<std::byte, 8>{}); });
    rejects([&] { (void)importObject3DDna(engine.reference(), load(dir/"invalid-table.dna")); });
    CHECK(engine.objectCount() == 0 && engine.rigCount() == 0);
    Object3DEngine bounded({16,16,1});
    auto first = importObject3DDna(bounded.reference(), bytes);
    rejects([&] { (void)importObject3DDna(bounded.reference(), bytes); });
    CHECK(bounded.objectCount() == 1 && bounded.rigCount() == 1 && first.rig.valid());
}
void rebind(const std::filesystem::path &dir)
{
    Object3DEngine engine;
    auto imported = importObject3DDna(engine.reference(), load(dir/"independent.dna"));
    auto editor = engine.editObject(imported.object);
    editor.get().moveJoint(1, {0,2,0});
    auto editedRig = imported.rig.get().cloneFor(editor.get().object());
    CHECK(near(editedRig.get().evaluate(std::array{.5}).joints[1].position.y, 2.005));
    CHECK(near(imported.rig.get().evaluate(std::array{.5}).joints[1].position.y, 1.005));
}
void interchange(const std::filesystem::path &dir)
{
    Object3DEngine engine;
    auto original=importObject3DDna(engine.reference(),load(dir/"independent.dna"));
    auto imported=importObject3DFbx(engine.reference(),exportObject3DFbx(original.object.get()));
    CHECK(imported.object.get().data().bones.size()==2);
    const auto &mesh=imported.object.get().data().meshes[0];
    CHECK(mesh.geometry.morphTargets.size()==1 && mesh.geometry.morphTargets[0].name=="Smile");
    CHECK(near(mesh.geometry.morphTargets[0].deltas[0].position.y,.2));
    CHECK(exportObject3DGlb(original.object.get()).size()>100);
}
void sourceRoundtrip(const std::filesystem::path &dir)
{
    Object3DEngine engine;
    for (const bool axes:{false,true})
    {
        const auto original=load(dir/(axes?"axes.dna":"independent.dna"));
        auto imported=importObject3DDna(engine.reference(),original);
        auto output=exportObject3DDna(imported.rig);
        auto decoded=importObject3DDna(engine.reference(),output.bytes);
        CHECK(imported.rig.get().sourceBytes()==original);
        const auto &a=*imported.object.get().data().meshes[0].topology;
        const auto &b=*decoded.object.get().data().meshes[0].topology;
        CHECK(a.layout->definition()==b.layout->definition());
        CHECK(near(decoded.rig.get().evaluate(std::array{.5}).joints[1].position.y,1.005));
        save(dir/(axes?"axes-roundtrip.dna":"roundtrip.dna"),output.bytes);
    }
}
void sourceEdited(const std::filesystem::path &dir)
{
    Object3DEngine engine;
    for (const bool axes:{false,true})
    {
        auto imported=importObject3DDna(engine.reference(),load(dir/(axes?"axes.dna":"independent.dna")));
        auto editor=engine.editObject(imported.object);
        editor.get().moveControlPoints(0,std::array{TopologyPointMove{0,{0,0,.025}}});
        editor.get().setControlWeights(0,std::array{TopologyPointWeights{0,{{0,.25},{1,.75}}}});
        editor.get().setControlMorph(0,{"Smile",{{0,{0,.3,0}}},0});
        editor.get().setTopologyTextureCoordinates(0,std::array{TopologyUvEdit{0,{.25,.75}}});
        editor.get().moveJoint(1,{0,1.25,0});
        editor.get().renameJoint(1,"chin");
        auto candidate=editor.get().object().get().data();
        candidate.bones[1].bindTransform.rotation=Quaternion::fromAxisAngle({0,0,1},.5);
        editor.get().replace(std::move(candidate));
        auto rig=imported.rig.get().cloneFor(editor.get().object(),std::array<BoneIndex,2>{0,1});
        auto output=exportObject3DDna(rig);
        auto decoded=importObject3DDna(engine.reference(),output.bytes);
        const auto &mesh=decoded.object.get().data().meshes[0];
        CHECK(near(mesh.topology->points[0].position.z,.025));
        CHECK(mesh.topology->points[0].weights.size()==2&&near(mesh.topology->points[0].weights[0].weight,.25));
        CHECK(mesh.topology->textureCoordinates[0]==TextureCoordinate(.25,.75));
        CHECK(near(mesh.topology->morphTargets[0].deltas[0].position.y,.3));
        CHECK(decoded.object.get().data().bones[1].name=="chin");
        const auto value=decoded.rig.get().evaluate(std::array{.5});
        CHECK(near(value.joints[1].position.y,1.255));
        CHECK(near(value.joints[1].rotation.z,std::sin((.5+7.5*std::numbers::pi/180)/2)));
        save(dir/(axes?"edited-axes.dna":"edited.dna"),output.bytes);
    }
    auto seam=importObject3DDna(engine.reference(),load(dir/"seam.dna"));
    auto editor=engine.editObject(seam.object);
    editor.get().moveVertices(0,std::array{VertexMove{1,{0,0,.02}}});
    editor.get().moveControlPoints(0,std::array{TopologyPointMove{6,{0,.01,0}}});
    editor.get().setTopologyTextureCoordinates(0,std::array{TopologyUvEdit{8,{.4,.6}}});
    editor.get().setControlMorph(0,{"Smile",{{1,{0,.2,0}},{6,{0,.04,0}}},0});
    auto rig=seam.rig.get().cloneFor(editor.get().object());
    const auto saved=exportObject3DDna(rig);
    auto decoded=importObject3DDna(engine.reference(),saved.bytes);
    const auto &mesh=decoded.object.get().data().meshes[0];
    CHECK(mesh.topology->points.size()==7&&mesh.geometry.vertices.size()==8);
    CHECK(mesh.topology->layout->definition().layouts.size()==9);
    CHECK(near(mesh.topology->points[6].position.y,1.51)&&mesh.topology->points[6].weights.empty());
    CHECK(near(mesh.topology->textureCoordinates[8].u,.4)&&mesh.topology->normals[2]==Vector3(0,0,0));
    CHECK(mesh.topology->morphTargets[0].deltas.size()==2);
    CHECK(near(mesh.geometry.vertices[1].position.z,.02)&&near(mesh.geometry.vertices[4].position.z,.02));
    save(dir/"seam-edited.dna",saved.bytes);
}
void sourceLayered(const std::filesystem::path &dir)
{
    Object3DEngine engine;auto source=importObject3DDna(engine.reference(),load(dir/"independent.dna"));
    Object3DTemplateDefinition definition;definition.base=source.object;
    definition.targets={{"shape",0,{{0,{0,0,.02}}}}};
    auto program=engine.createTemplate(std::move(definition));auto editor=engine.editTemplate(program);auto &e=editor.get();
    e.moveControlPoints(0,std::array{TopologyPointMove{0,{0,0,.005}}});
    e.setControlMorph(0,{"Smile",{{0,{0,.3,0}}},0});e.setLayerWeight(e.layers().back().id,.5);
    e.setControlWeights(0,std::array{TopologyPointWeights{0,{{0,.25},{1,.75}}}});
    e.setTopologyTextureCoordinates(0,std::array{TopologyUvEdit{0,{.25,.75}}});
    e.moveJoint(1,{0,1.1,0});e.setTemplateValues(std::array{TemplateValue{"shape",.5}});
    auto rig=source.rig.get().cloneFor(e.object());auto output=exportObject3DDna(rig);
    auto decoded=importObject3DDna(engine.reference(),output.bytes);const auto &t=*decoded.object.get().data().meshes[0].topology;
    CHECK(near(t.points[0].position.z,.015));CHECK(near(t.morphTargets[0].deltas[0].position.y,.25));
    CHECK(t.textureCoordinates[0]==TextureCoordinate(.25,.75));
    CHECK(t.points[0].weights.size()==2 && near(t.points[0].weights[0].weight,.25));
    CHECK(near(decoded.rig.get().evaluate(std::array{.5}).joints[1].position.y,1.105));
    save(dir/"layered.dna",output.bytes);
}
void sourceFbx(const std::filesystem::path &dir)
{
    Object3DEngine engine;auto source=importObject3DDna(engine.reference(),load(dir/"independent.dna"));
    auto edit=engine.editObject(source.object);edit.get().moveControlPoints(0,std::array{TopologyPointMove{0,{0,0,.01}}});
    auto imported=importObject3DFbx(engine.reference(),exportObject3DFbx(edit.get().object().get()));
    CHECK(imported.object.get().data().meshes[0].topology->layout->definition()==source.object.get().data().meshes[0].topology->layout->definition());
    auto rig=source.rig.get().cloneFor(imported.object);auto output=exportObject3DDna(rig);
    auto result=importObject3DDna(engine.reference(),output.bytes);
    CHECK(near(result.object.get().data().meshes[0].topology->points[0].position.z,.01));
    CHECK(near(result.rig.get().evaluate(std::array{.5}).joints[1].position.y,1.005));
    save(dir/"fbx-edited.dna",output.bytes);
}
void sourceGroupedFbx(const std::filesystem::path &dir)
{
    Object3DEngine engine;auto source=importObject3DDna(engine.reference(),load(dir/"grouped-source.dna"));
    const auto &original=source.object.get().data().meshes[0].topology->morphTargets;
    CHECK(original.size()==2&&original[0].name=="Smile"&&original[1].name=="Smile");
    CHECK(original[0].sourceTarget==0&&original[1].sourceTarget==1);
    FbxExportOptions output;output.externalDocument="dna-grouped-external";
    FbxImportOptions input;input.restoreAuthoringMetadata=false;
    auto baseline=importObject3DFbx(engine.reference(),exportObject3DFbx(source.object.get(),output),input);
    CHECK(baseline.object.get().data().meshes[0].topology->morphTargets.size()==1);
    auto editedData=baseline.object.get().data();auto &shape=editedData.meshes[0].topology->morphTargets[0];
    for(auto &normal:editedData.meshes[0].topology->normals)normal={.6,0,.8};
    auto point=std::find_if(shape.deltas.begin(),shape.deltas.end(),[](const auto &v){return v.point==0;});
    CHECK(point!=shape.deltas.end());point->position.y+=.05;
    auto edited=engine.createObject(std::move(editedData));auto editor=engine.editObject(source.object);
    Object3DExternalOptions options;options.document=output.externalDocument;
    const auto report=editor.get().applyExternalEdits(source.object.get(),{baseline.object.get(),baseline.externalIdentity},
        {edited.get(),baseline.externalIdentity},options);
    CHECK(report.morphs==1&&report.normals==1);
    const auto &records=editor.get().object().get().data().meshes[0].topology->morphTargets;
    CHECK(records.size()==2&&records[0].sourceTarget==0&&records[1].sourceTarget==1);
    CHECK(near(records[0].deltas[0].position.y,.25)&&near(records[1].deltas[0].position.y,.1));
    auto rig=source.rig.get().cloneFor(editor.get().object());auto saved=exportObject3DDna(rig);
    auto restored=importObject3DDna(engine.reference(),saved.bytes);
    CHECK(near(restored.rig.get().evaluate(std::array{.5}).joints[1].position.y,1.005));
    save(dir/"grouped-fbx-edited.dna",saved.bytes);
}
void sourceLods(const std::filesystem::path &dir)
{
    Object3DEngine engine;const auto bytes=load(dir/"multi-lod.dna");
    auto high=importObject3DDna(engine.reference(),bytes);
    DnaExportOptions strict;strict.requireAllMeshes=true;
    rejects([&] { (void)exportObject3DDna(high.rig,{},strict); });
    auto partial=exportObject3DDna(high.rig);CHECK(!partial.warnings.empty());
    DnaImportOptions lowOptions;lowOptions.geometryLod=1;
    auto low=importObject3DDna(engine.reference(),bytes,lowOptions);
    auto eh=engine.editObject(high.object),el=engine.editObject(low.object);
    eh.get().moveControlPoints(0,std::array{TopologyPointMove{0,{0,0,.02}}});
    el.get().moveControlPoints(0,std::array{TopologyPointMove{1,{0,0,.03}}});
    eh.get().moveJoint(1,{0,1.2,0});
    auto rh=high.rig.get().cloneFor(eh.get().object()),rl=low.rig.get().cloneFor(el.get().object());
    rejects([&] { (void)exportObject3DDna(rh,std::array{rl},strict); });
    el.get().moveJoint(1,{0,1.2,0});rl=low.rig.get().cloneFor(el.get().object());
    const auto full=exportObject3DDna(rh,std::array{rl},strict);
    CHECK(full.warnings.empty());
    const auto a=importObject3DDna(engine.reference(),full.bytes);
    const auto b=importObject3DDna(engine.reference(),full.bytes,lowOptions);
    CHECK(near(a.object.get().data().meshes[0].topology->points[0].position.z,.02));
    CHECK(near(b.object.get().data().meshes[0].topology->points[1].position.z,.03));
    CHECK(near(b.object.get().data().bones[1].bindTransform.position.y,1.2));
    save(dir/"multi-lod-edited.dna",full.bytes);
}
void sourceValidation(const std::filesystem::path &dir)
{
    Object3DEngine engine;auto source=importObject3DDna(engine.reference(),load(dir/"independent.dna"));
    auto profileEditor=engine.editObject(source.object);
    profileEditor.get().setSkinningProfile(0,{SkinningMethod::DualQuaternion,{}});
    auto profiled=source.rig.get().cloneFor(profileEditor.get().object());
    rejects([&]{(void)exportObject3DDna(profiled);});
    rejects([&]{(void)profiled.get().serializeSource();});
    rejects([&]{(void)exportObject3DDna(source.rig,std::array{profiled});});
    DnaExportOptions limits;limits.maximumMemory=1024;
    rejects([&] { (void)exportObject3DDna(source.rig,{},limits); });
    limits={};limits.maximumBytes=1;
    rejects([&] { (void)exportObject3DDna(source.rig,{},limits); });
    auto editor=engine.editObject(source.object);
    editor.get().setControlMorph(0,{"NotInRig",{{0,{0,.01,0}}},NoTopologyIndex});
    auto extra=source.rig.get().cloneFor(editor.get().object());
    rejects([&] { (void)exportObject3DDna(extra); });
    auto modified=source.object.get().data();modified.bones[1].bindTransform.scale={2,1,1};
    editor.get().replace(std::move(modified));auto scaled=source.rig.get().cloneFor(editor.get().object());
    rejects([&] { (void)exportObject3DDna(scaled); });
    auto changed=source.object.get().data();
    auto definition=changed.meshes[0].topology->layout->definition();
    std::rotate(definition.polygons[0].begin(),definition.polygons[0].begin()+1,definition.polygons[0].end());
    changed.meshes[0].topology->layout=MeshTopologyLayout::create(definition,changed.meshes[0].topology->points);
    editor.get().replace(std::move(changed));
    auto retopology=source.rig.get().cloneFor(editor.get().object());
    rejects([&] { (void)exportObject3DDna(retopology); });
    Object3DEngine other;
    auto foreign=importObject3DDna(other.reference(),load(dir/"independent.dna"));
    rejects([&] { (void)exportObject3DDna(source.rig,std::array{foreign.rig}); });
    auto roots=importObject3DDna(engine.reference(),load(dir/"roots.dna"));
    CHECK(roots.object.get().data().bones.size()==3);
    auto rootEditor=engine.editObject(roots.object);
    auto rootData=roots.object.get().data();
    rootData.bones[0].bindTransform.rotation={0,0,0,-1};
    rootEditor.get().replace(std::move(rootData));
    auto rebound=roots.rig.get().cloneFor(rootEditor.get().object());
    auto saved=exportObject3DDna(rebound);
    auto decoded=importObject3DDna(engine.reference(),saved.bytes);
    const auto jaw=decoded.object.get().skeleton()->findBone("jaw");
    CHECK(near(decoded.rig.get().evaluate(std::array{.5}).joints[jaw].position.y,1.005));
    save(dir/"roots-roundtrip.dna",saved.bytes);
    auto channel=importObject3DDna(engine.reference(),load(dir/"extra-channel.dna"));
    auto channelEditor=engine.editObject(channel.object);
    channelEditor.get().setControlMorph(0,{"Blink",{{1,{.01,0,0}}}});
    auto channelRig=channel.rig.get().cloneFor(channelEditor.get().object());
    auto added=exportObject3DDna(channelRig);
    auto addedRead=importObject3DDna(engine.reference(),added.bytes);
    const auto &targets=addedRead.object.get().data().meshes[0].topology->morphTargets;
    CHECK(targets.size()==2&&targets[0].sourceTarget==0&&targets[1].sourceTarget==1);
    CHECK(targets[1].name=="Blink"&&near(targets[1].deltas[0].position.x,.01));
    save(dir/"extra-channel-edited.dna",added.bytes);
    CHECK(source.rig.valid()&&near(source.rig.get().evaluate(std::array{.5}).joints[1].position.y,1.005));
    CHECK(!exportObject3DDna(source.rig).bytes.empty());
}
void sourceNormals(const std::filesystem::path &dir)
{
    Object3DEngine engine;auto source=importObject3DDna(engine.reference(),load(dir/"independent.dna"));
    auto editor=engine.editObject(source.object);auto morph=source.object.get().data().meshes[0].topology->morphTargets.at(0);
    morph.normalDeltas=topologyMorphNormalDeltas(*source.object.get().data().meshes[0].topology,morph);
    editor.get().setControlMorph(0,morph);auto same=source.rig.get().cloneFor(editor.get().object());
    CHECK(exportObject3DDna(same).warnings.empty());
    morph.normalDeltas=std::vector<TopologyNormalDelta>{{0,{.2,0,0}}};editor.get().setControlMorph(0,morph);
    auto custom=source.rig.get().cloneFor(editor.get().object());rejects([&]{(void)exportObject3DDna(custom);});
    DnaExportOptions options;options.allowDiscardMorphNormals=true;auto discarded=exportObject3DDna(custom,{},options);
    CHECK(!discarded.warnings.empty());auto restored=importObject3DDna(engine.reference(),discarded.bytes);
    CHECK(!restored.object.get().data().meshes[0].topology->morphTargets[0].normalDeltas);
    CHECK(near(restored.rig.get().evaluate(std::array{.5}).joints[1].position.y,1.005));
}
void sourceProject(const std::filesystem::path &dir)
{
    Object3DEngine engine;auto source=importObject3DDna(engine.reference(),load(dir/"independent.dna"));
    Object3DTemplateDefinition definition;definition.base=source.object;definition.targets={{"shape",0,{{0,{0,0,.02}}}}};
    auto editor=engine.editTemplate(engine.createTemplate(std::move(definition)),std::array{TemplateValue{"shape",.5}});
    editor.get().setTopologyTextureCoordinates(0,std::array{TopologyUvEdit{0,{.25,.75}}});auto rig=source.rig.get().cloneFor(editor.get().object());
    const auto bytes=exportObject3DProject(editor,std::array{rig});Object3DEngine restored;auto project=importObject3DProject(restored.reference(),bytes);
    CHECK(project.rigs.size()==1&&project.rigs[0].get().object()==project.editor.get().object());
    rejects([&]{(void)exportObject3DProject(editor,std::array{project.rigs[0]});});
    rejects([&]{(void)exportObject3DProject(editor,std::array{rig,rig});});
    CHECK(project.rigs[0].get().sourceBytes()==source.rig.get().sourceBytes());
    CHECK(near(project.rigs[0].get().evaluate(std::array{.5}).joints[1].position.y,1.005));
    CHECK(near(project.editor.get().object().get().data().meshes[0].topology->textureCoordinates[0].u,.25));
    auto dna=exportObject3DDna(project.rigs[0]);save(dir/"project-edited.dna",dna.bytes);
    const std::array baseline{TemplateValue{"shape",.5}};
    const std::array sample{TemplateMorphSample{"shape",.8,"shape_generated"}};
    const auto baked=bakeObject3DTemplateMorphs(editor.get().templateProgram(),baseline,sample);
    const auto morph=createObject3DTemplateMorphRig(baked);
    const auto combined=exportObject3DProject(editor,std::array{rig,morph});CHECK(combined[8]==21);
    Object3DEngine combinedEngine;const auto combinedProject=importObject3DProject(combinedEngine.reference(),combined);
    CHECK(combinedProject.rigs.size()==2&&combinedProject.rigs[0].get().sourceBytes()==source.rig.get().sourceBytes());
    CHECK(near(combinedProject.rigs[0].get().evaluate(std::array{.5}).joints[1].position.y,1.005));
    const auto morphInfo=inspectObject3DTemplateMorphRig(combinedProject.rigs[1]);
    CHECK(morphInfo.templateProgram.valid()&&morphInfo.program.baselineInputs&&morphInfo.program.channels[0].name=="shape_generated");
    project.editor.get().setTemplateValues(std::array{TemplateValue{"shape",.25}});
    CHECK(near(project.editor.get().object().get().data().meshes[0].topology->points[0].position.z,.005));
    CHECK(near(project.editor.get().object().get().data().meshes[0].topology->textureCoordinates[0].u,.25));
    auto high=importObject3DDna(engine.reference(),load(dir/"multi-lod.dna"));DnaImportOptions lowOptions;lowOptions.geometryLod=1;
    auto low=importObject3DDna(engine.reference(),load(dir/"multi-lod.dna"),lowOptions);auto lowEdit=engine.editObject(low.object);
    const auto lods=exportObject3DProject(lowEdit,std::array{high.rig,low.rig});Object3DEngine other;auto loaded=importObject3DProject(other.reference(),lods);
    CHECK(loaded.rigs.size()==2&&loaded.rigs[0].get().sourceBytes()==loaded.rigs[1].get().sourceBytes());
    for(std::size_t lod=0;lod<2;++lod)CHECK(near(loaded.rigs[1].get().evaluate(std::array{.5},RigControlSpace::Raw,lod).joints[1].position.y,low.rig.get().evaluate(std::array{.5},RigControlSpace::Raw,lod).joints[1].position.y));
    std::ofstream f(dir/"Face.ii3dproject",std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(f.good());
}
void uvLoss(const std::filesystem::path &dir)
{
    Object3DEngine engine;auto imported=importObject3DDna(engine.reference(),load(dir/"independent.dna"));
    auto data=imported.object.get().data();auto &topology=*data.meshes[0].topology;
    TopologyTextureCoordinateSet extra;extra.name="Additional";extra.coordinates=topology.textureCoordinates;
    for(const auto &layout:topology.layout->definition().layouts)extra.layoutIndices.push_back(layout.textureCoordinate);
    topology.additionalUvSets.push_back(std::move(extra));
    auto editor=engine.editObject(imported.object);editor.get().replace(std::move(data));
    auto rig=imported.rig.get().cloneFor(editor.get().object());
    rejects([&]{(void)exportObject3DDna(rig);});
    DnaExportOptions options;options.allowDiscardAdditionalUvSets=true;
    const auto saved=exportObject3DDna(rig,{},options);
    CHECK(std::any_of(saved.warnings.begin(),saved.warnings.end(),[](const auto &v){return v.find("additional UV sets")!=std::string::npos;}));
    const auto decoded=importObject3DDna(engine.reference(),saved.bytes);
    CHECK(decoded.object.get().data().meshes[0].topology->additionalUvSets.empty());
}
void sourceReference(const std::filesystem::path &dir)
{
    Object3DEngine engine;auto imported=importObject3DDna(engine.reference(),load(dir/"independent.dna"));auto editor=engine.editObject(imported.object);
    PoseReference reference;reference.directions={{0,1,{1,0,0}}};editor.get().setReferencePose(reference);auto rig=imported.rig.get().cloneFor(editor.get().object());
    rejects([&]{(void)exportObject3DDna(rig);});rejects([&]{(void)rig.get().serializeSource();});rejects([&]{(void)exportObject3DDna(imported.rig,std::array{rig});});
    auto bytes=exportObject3DProject(editor,std::array{rig});CHECK(bytes[8]==13);Object3DEngine target;auto copy=importObject3DProject(target.reference(),bytes);CHECK(copy.rigs.size()==1&&copy.editor.get().object().get().data().referencePose.has_value());
    CHECK(copy.rigs[0].get().sourceBytes()==imported.rig.get().sourceBytes());editor.get().setReferencePose(std::nullopt);auto neutral=rig.get().cloneFor(editor.get().object());
    const auto output=exportObject3DDna(neutral);auto decoded=importObject3DDna(engine.reference(),output.bytes);CHECK(!decoded.object.get().data().referencePose&&decoded.object.get().data().bones.size()==imported.object.get().data().bones.size());
    const auto name=editor.get().object().get().data().meshes[0].geometry.morphTargets[0].name;
    editor.get().setDefaultMorphWeight(0,name,.25);auto weighted=neutral.get().cloneFor(editor.get().object());
    rejects([&]{(void)exportObject3DDna(weighted);});
    rejects([&]{(void)exportObject3DDna(imported.rig,std::array{weighted});});
    editor.get().setDefaultMorphWeight(0,name,0);auto cleared=weighted.get().cloneFor(editor.get().object());
    CHECK(!exportObject3DDna(cleared).bytes.empty());
}
int main(int argc, char **argv)
{
    try
    {
        if (argc != 3) return 2;
        const std::string name = argv[1];
        const std::filesystem::path path(argv[2]);
        if (name == "import") imported(path);
        else if (name == "controls") controls(path);
        else if (name == "progressive_project") progressiveProject(path);
        else if (name == "axes") axes(path);
        else if (name == "limits") limits(path);
        else if (name == "rebind") rebind(path);
        else if (name == "uv_loss") uvLoss(path);
        else if (name == "interchange") interchange(path);
        else if (name == "source_roundtrip") sourceRoundtrip(path);
        else if (name == "source_edited") sourceEdited(path);
        else if (name == "source_layered") sourceLayered(path);
        else if (name == "source_fbx") sourceFbx(path);
        else if (name == "source_grouped_fbx") sourceGroupedFbx(path);
        else if (name == "source_lods") sourceLods(path);
        else if (name == "source_validation") sourceValidation(path);
        else if (name == "source_normals") sourceNormals(path);
        else if (name == "source_project") sourceProject(path);
        else if (name == "source_reference") sourceReference(path);
        else return 2;
        std::cout << name << " passed\n";
    }
    catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
