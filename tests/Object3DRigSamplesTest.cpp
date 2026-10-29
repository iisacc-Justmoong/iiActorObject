#include <iiActorObject/Object3DRigSamples.h>
#include <iiActorObject/RigSampleCatalogExport.h>
#include <iiActorObject/Object3DPoseRig.h>
#include <iiActorObject/Object3DDna.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/RigSamples.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace iiActorObject;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while(false)
template<class F> void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
void same(Vector3 a,Vector3 b){CHECK(std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)<3e-9);}
struct Metrics final {std::size_t clones=0,sourceCalls=0,cloneCalls=0;};
struct Driver final:Object3DRigDriver
{
    Object3DRigDefinition info;std::shared_ptr<Metrics> metrics;bool cloned=false;std::size_t calls=0;
    explicit Driver(std::shared_ptr<Metrics> m,bool partial=false):metrics(std::move(m))
    {
        info.sourceFormat="test-driver";info.joints={"root","tip","link"};info.rawControls={"x","aux"};info.guiControls={"x","aux"};
        info.rawControlDefaults={.3,.25};info.guiControlDefaults={.7,.1};info.blendShapes={"Unused","Bend","Lift"};
        info.animatedMaps={"wrinkle"};info.lodCount=2;if(partial)info.drivenJoints=std::vector<std::size_t>{1};
    }
    const Object3DRigDefinition &definition()const noexcept override{return info;}
    std::unique_ptr<Object3DRigDriver> clone()const override
    {++metrics->clones;auto result=std::make_unique<Driver>(*this);result->cloned=true;return result;}
    Object3DRigOutputs evaluate(std::span<const double> values,RigControlSpace space,std::size_t lod)override
    {
        ++calls;if(cloned)++metrics->cloneCalls;else ++metrics->sourceCalls;
        const auto x=values[0]*(space==RigControlSpace::Gui?2:1)+values[1]+.001*calls;
        Object3DRigOutputs out;out.joints.resize(3);
        if(lod==0){out.joints[1].translation.y=.05*x;out.joints[1].rotation=Quaternion::fromAxisAngle({0,1,0},.2*x);out.joints[1].scale={.03*x,0,0};}
        out.blendShapes={.9,lod==0?-.2*x:0,lod==0?x:0};out.animatedMaps={lod==0?x*x:0};return out;
    }
};
Object3DData fixture(bool reference=false)
{
    Object3DData data;data.bones={{"root",NoBone,{}},{"tip",2,{{.2,.3,.1}}},{"link",0,{{0,.4,0}}}};
    Object3DMesh raw;raw.name="Raw";raw.geometry.vertices={{{0,.5,0},{.3,.4,1},{{1,.3},{2,.7}}},
        {{.8,.5,0},{0,0,1},{{2,1}}},{{.8,1,0},{.1,.2,1},{{0,.6},{1,.4}}}};raw.geometry.triangles={0,1,2};
    raw.geometry.morphTargets={{"Lift",{{0,{0,.2,0},{.2,.1,0}}}},{"Custom",{{1,{.1,0,0},{0,.1,.1}}}},
        {"Bend",{{0,{.1,0,.2},{.1,-.1,0}},{2,{0,.1,0},{0,.1,0}}}}};data.meshes.push_back(std::move(raw));
    MeshTopology t;
    t.points={{{0,1,0},{{1,1}}},{{.1,1,0},{{1,1}}},{{.2,1,0},{{1,1}}},{{0,1.1,0},{{1,1}}},
              {{.1,1.1,0},{{1,1}}},{{.2,1.1,0},{{1,1}}},{{2,2,2},{}},{{3,3,3},{{1,1}}}};
    t.normals={{0,0,1},{0,0,1}};t.textureCoordinates={{0,0},{1,0},{1,1},{0,1},{0,0},{1,0},{1,1},{0,1}};
    MeshTopologyDefinition d;d.sourceFormat="seams";d.positionCount=8;d.normalCount=2;d.textureCoordinateCount=8;
    d.layouts={{0,0,0},{1,1,0},{4,2,0},{3,3,0},{1,4,1},{2,5,1},{5,6,1},{4,7,1},{7,0,1}};
    d.polygons={{0,1,2,3},{4,5,6,7}};t.layout=MeshTopologyLayout::create(std::move(d),t.points);
    TopologyMorph lift{"Lift",{{1,{0,.02,0}},{6,{.2,0,0}},{7,{0,.2,0}}},0};
    lift.normalDeltas=std::vector<TopologyNormalDelta>{{8,{.2,0,0}}};
    t.morphTargets.push_back(std::move(lift));t.morphTargets.push_back({"Bend",{{1,{.01,0,.01}}},1});
    TopologyMorph extra{"Lift",{{6,{.1,0,0}}},2};extra.normalDeltas=std::vector<TopologyNormalDelta>{{8,{.05,0,0}}};
    t.morphTargets.push_back(std::move(extra));
    Object3DMesh seam;seam.name="Seam";seam.topology=std::move(t);data.meshes.push_back(std::move(seam));
    if(reference){data.referencePose=PoseReference{};data.referencePose->directions={{2,1,{0,1,0}}};}
    return data;
}
RigSampleInput input(RigControlSpace space,double x){return {space,{{"x",x}}};}
void compare(const Object3DRigHandle &rig,const Object3DHandle &output,const RigSampleInput &controls,std::string_view morph,std::size_t lod)
{
    auto clone=rig.get().cloneFor(rig.get().object());std::vector<NamedRigControl> named;
    for(const auto &v:controls.controls)named.push_back({v.name,v.value});
    const auto value=clone.get().evaluateNamed(named,controls.space,lod);auto actor=rig.get().object().get().instantiate();clone.get().applyTo(*actor,value);
    Pose rest(output.get().skeleton());std::size_t mesh=0;
    for(const auto *component:actor->components())if(const auto *part=dynamic_cast<const SkeletalMeshComponent*>(component))
    {
        const auto expected=part->deformedVertices();const auto &target=*output.get().meshes()[mesh];const auto &geometry=target.lods()[0];
        std::vector<double> weights(geometry.morphTargets.size());bool found=morph.empty();
        for(std::size_t i=0;i<weights.size();++i)if(geometry.morphTargets[i].name==morph){weights[i]=1;found=true;}
        CHECK(found);std::vector<DeformedVertex> actual(expected.size());target.deform(rest,0,weights,actual);
        for(std::size_t v=0;v<actual.size();++v){same(expected[v].position,actual[v].position);same(expected[v].normal,actual[v].normal);}++mesh;
    }
    CHECK(mesh==output.get().meshes().size());
}
void endpoints(bool partial=false)
{
    Object3DEngine engine;const auto source=engine.createObject(fixture(partial));auto metrics=std::make_shared<Metrics>();
    const auto rig=engine.createRigSession(source,std::make_unique<Driver>(metrics,partial));
    const auto base=input(RigControlSpace::Raw,.3);const std::array requests{RigMorphRequest{"RawSmile",input(RigControlSpace::Raw,.8)},
        RigMorphRequest{"GuiSmile",input(RigControlSpace::Gui,.2)},RigMorphRequest{"Same",base}};
    for(std::size_t lod=0;lod<2;++lod)
    {
        RigMorphSampleOptions options;options.lod=lod;const auto before=metrics->clones;
        const auto out=createObject3DRigMorphSamples(rig,base,requests,options);
        CHECK(metrics->clones-before==requests.size()+1&&metrics->cloneCalls==metrics->clones&&metrics->sourceCalls==0);
        CHECK(engine.rigCount()>=1&&out.id()!=source.id());
        const auto &d=out.get().data();CHECK(d.rigSampleCatalogs.size()==1);const auto &catalog=d.rigSampleCatalogs[0];
        CHECK(catalog.lod==lod&&catalog.sourceFormat=="test-driver"&&catalog.samples.size()==3);
        CHECK(catalog.baseline.controls==std::vector<double>({.3,.25}));CHECK(catalog.samples[1].state.controls==std::vector<double>({.2,.1}));
        CHECK(catalog.baseline.animatedMaps.size()==1&&catalog.samples[0].state.sourceLocals.size()==3);
        const auto &topology=*d.meshes[1].topology;const auto &original=*source.get().data().meshes[1].topology;
        const auto &beforeLayout=original.layout->definition(),&afterLayout=topology.layout->definition();
        CHECK(topology.points.size()==original.points.size()&&beforeLayout.layouts.size()==afterLayout.layouts.size());
        CHECK(beforeLayout.polygons==afterLayout.polygons&&beforeLayout.renderToLayout==afterLayout.renderToLayout);
        for(std::size_t i=0;i<beforeLayout.layouts.size();++i)
            CHECK(beforeLayout.layouts[i].position==afterLayout.layouts[i].position&&beforeLayout.layouts[i].textureCoordinate==afterLayout.layouts[i].textureCoordinate);
        CHECK(topology.normals.size()>=original.normals.size()&&std::equal(original.normals.begin(),original.normals.end(),topology.normals.begin()));
        CHECK(topology.textureCoordinates==original.textureCoordinates&&original.morphTargets.size()==3&&topology.morphTargets.size()==6);
        CHECK(topology.morphTargets.back().normalDeltas.has_value());
        compare(rig,out,base,"",lod);for(const auto &request:requests)compare(rig,out,request.input,request.morph,lod);
        const auto &sample=topology.morphTargets[topology.morphTargets.size()-3];CHECK(sample.name=="RawSmile");
        const auto point=[&](std::uint32_t id){Vector3 result=topology.points[id].position;for(const auto &delta:sample.deltas)if(delta.point==id)
            {result.x+=delta.position.x;result.y+=delta.position.y;result.z+=delta.position.z;}return result;};
        const auto lift=catalog.samples[0].state.blendShapes[2];same(point(6),{2+.3*lift,2,2});
        auto checkRig=rig.get().cloneFor(source);const auto evaluation=checkRig.get().evaluateNamed(std::array{NamedRigControl{"x",.8}},RigControlSpace::Raw,lod);
        auto actor=source.get().instantiate();checkRig.get().applyTo(*actor,evaluation);
        same(point(7),actor->pose().skinMatrices()[1].transformPoint({3,3+.2*lift,3}));
        const auto normalize=[](Vector3 v){const auto length=std::hypot(v.x,v.y,v.z);CHECK(length>0);return Vector3{v.x/length,v.y/length,v.z/length};};
        const auto expectedNormal=normalize(actor->pose().normalMatrices()[1].transformVector({.25*lift,0,1}));
        Vector3 actualNormal=topology.normals[afterLayout.layouts[8].normal];
        for(const auto &delta:*sample.normalDeltas)if(delta.layout==8)
            {actualNormal.x+=delta.normal.x;actualNormal.y+=delta.normal.y;actualNormal.z+=delta.normal.z;}
        same(normalize(actualNormal),expectedNormal);
        CHECK(metrics->sourceCalls==0);
    }
}
Object3DData nonlinearFixture(SkinningMethod method)
{
    auto data=fixture();auto &topology=*data.meshes[1].topology;
    for(std::size_t p=0;p<topology.points.size();++p)
        if(p!=6)topology.points[p].weights={{0,.5},{1,.5}};
    topology.points[0].weights.clear(); // Visible, unweighted point uses the root.
    topology.skinning={method,method==SkinningMethod::Blended?std::vector<double>{.4,0,.4,.6,.8,1,.3,.65}:std::vector<double>{}};
    data.meshes[0].geometry.skinning={method,method==SkinningMethod::Blended?std::vector<double>{0,.4,1}:std::vector<double>{}};
    // A rendered proxy gives an independent runtime oracle for an unrendered
    // source point and its separate normal layout.
    Object3DMesh proxy;proxy.name="HiddenProxy";
    proxy.geometry.vertices={{{3,3,3},{0,0,1},{{0,.5},{1,.5}}},
                             {{4,3,3},{0,0,1},{{0,.5},{1,.5}}},
                             {{3,4,3},{0,0,1},{{0,.5},{1,.5}}}};
    proxy.geometry.triangles={0,1,2};proxy.geometry.morphTargets={{"Lift",{{0,{0,.2,0},{.25,0,0}}}}};
    proxy.geometry.skinning={method,method==SkinningMethod::Blended?std::vector<double>{.65,.5,.5}:std::vector<double>{}};
    data.meshes.push_back(std::move(proxy));return data;
}
void nonlinear()
{
    for(const auto method:{SkinningMethod::DualQuaternion,SkinningMethod::Blended})
    {
        Object3DEngine engine;const auto source=engine.createObject(nonlinearFixture(method));
        auto metrics=std::make_shared<Metrics>();const auto rig=engine.createRigSession(source,std::make_unique<Driver>(metrics));
        const auto baseline=input(RigControlSpace::Raw,.3);
        const std::array requests{RigMorphRequest{"RawSmile",input(RigControlSpace::Raw,.8)},
                                  RigMorphRequest{"GuiSmile",input(RigControlSpace::Gui,.2)}};
        const auto count=engine.objectCount();
        rejects([&]{(void)createObject3DRigMorphSamples(rig,baseline,requests,{},static_cast<Object3DSnapshotSkinning>(99));});
        CHECK(metrics->clones==0&&engine.objectCount()==count);
        RigMorphSampleOptions limited;limited.maximumWork=1;
        rejects([&]{(void)createObject3DRigMorphSamples(rig,baseline,requests,limited);});
        CHECK(engine.objectCount()==count);
        for(const auto policy:{Object3DSnapshotSkinning::Preserve,Object3DSnapshotSkinning::LinearizeAtBind})
        {
            const auto out=policy==Object3DSnapshotSkinning::Preserve
                ?createObject3DRigMorphSamples(rig,baseline,requests)
                :createObject3DRigMorphSamples(rig,baseline,requests,{},policy);
            const auto expectedMethod=policy==Object3DSnapshotSkinning::Preserve?method:SkinningMethod::Linear;
            CHECK(out.get().data().meshes[0].geometry.skinning.method==expectedMethod);
            CHECK(out.get().data().meshes[1].topology->skinning.method==expectedMethod);
            compare(rig,out,baseline,"",0);
            for(const auto &request:requests)compare(rig,out,request.input,request.morph,0);
            if(policy==Object3DSnapshotSkinning::Preserve)
            {
                const auto explicitPreserve=createObject3DRigMorphSamples(rig,baseline,requests,{},policy);
                compare(rig,explicitPreserve,requests[0].input,requests[0].morph,0);
            }
            if(policy==Object3DSnapshotSkinning::Preserve)
                rejects([&]{(void)exportObject3DGlb(out.get());});
            else CHECK(!exportObject3DGlb(out.get()).empty());
            Object3DEngine restoredEngine;const auto editor=engine.editObject(out);
            const auto restored=importObject3DProject(restoredEngine.reference(),exportObject3DProject(editor));
            CHECK(restored.editor.get().object().get().data().meshes[1].topology->skinning.method==expectedMethod);
            compare(rig,restored.editor.get().object(),requests[0].input,requests[0].morph,0);

            auto clone=rig.get().cloneFor(source);
            const auto evaluation=clone.get().evaluateNamed(std::array{NamedRigControl{"x",.8}},RigControlSpace::Raw,0);
            auto actor=source.get().instantiate();clone.get().applyTo(*actor,evaluation);
            std::vector<const SkeletalMeshComponent*> parts;
            for(const auto *component:actor->components())
                if(const auto *part=dynamic_cast<const SkeletalMeshComponent*>(component))parts.push_back(part);
            CHECK(parts.size()==3);const auto expected=parts[2]->deformedVertices()[0];
            const auto &topology=*out.get().data().meshes[1].topology;
            const auto &sample=topology.morphTargets[topology.morphTargets.size()-2];
            auto point=topology.points[7].position;
            for(const auto &delta:sample.deltas)if(delta.point==7)
                {point.x+=delta.position.x;point.y+=delta.position.y;point.z+=delta.position.z;}
            same(point,expected.position);
            const auto normalId=topology.layout->definition().layouts[8].normal;
            auto normal=topology.normals[normalId];
            for(const auto &delta:*sample.normalDeltas)if(delta.layout==8)
                {normal.x+=delta.normal.x;normal.y+=delta.normal.y;normal.z+=delta.normal.z;}
            const auto length=std::hypot(normal.x,normal.y,normal.z);CHECK(length>0);
            same({normal.x/length,normal.y/length,normal.z/length},expected.normal);
            auto untouched=topology.points[6].position;
            for(const auto &delta:sample.deltas)if(delta.point==6)
                {untouched.x+=delta.position.x;untouched.y+=delta.position.y;untouched.z+=delta.position.z;}
            const auto lift=out.get().data().rigSampleCatalogs[0].samples[0].state.blendShapes[2];
            same(untouched,{2+.3*lift,2,2});
        }
    }
}
void failures()
{
    Object3DEngine engine;const auto source=engine.createObject(fixture());auto metrics=std::make_shared<Metrics>();
    const auto rig=engine.createRigSession(source,std::make_unique<Driver>(metrics));const auto base=input(RigControlSpace::Raw,.3);
    std::array requests{RigMorphRequest{"One",input(RigControlSpace::Raw,.8)}};const auto count=engine.objectCount();
    rejects([&]{(void)createObject3DRigMorphSamples(rig,base,{});});
    auto bad=requests;bad[0].morph="Lift";rejects([&]{(void)createObject3DRigMorphSamples(rig,base,bad);});
    bad=requests;bad[0].input.controls={{"unknown",1}};rejects([&]{(void)createObject3DRigMorphSamples(rig,base,bad);});
    bad=requests;bad[0].input.controls={{"x",1},{"x",2}};rejects([&]{(void)createObject3DRigMorphSamples(rig,base,bad);});
    bad=requests;bad[0].input.controls={{"x",std::numeric_limits<double>::infinity()}};rejects([&]{(void)createObject3DRigMorphSamples(rig,base,bad);});
    bad=requests;bad[0].input.space=static_cast<RigControlSpace>(99);rejects([&]{(void)createObject3DRigMorphSamples(rig,base,bad);});
    RigMorphSampleOptions options;options.maximumWork=1;rejects([&]{(void)createObject3DRigMorphSamples(rig,base,requests,options);});
    options={};options.maximumAddedDeltas=1;rejects([&]{(void)createObject3DRigMorphSamples(rig,base,requests,options);});
    options={};options.lod=2;rejects([&]{(void)createObject3DRigMorphSamples(rig,base,requests,options);});
    CHECK(engine.objectCount()==count&&engine.rigCount()==1&&metrics->sourceCalls==0);
}
void textBudget()
{
    Object3DEngine engine;const auto source=engine.createObject(fixture());auto metrics=std::make_shared<Metrics>();
    auto driver=std::make_unique<Driver>(metrics);driver->info.rawControls.clear();driver->info.rawControlDefaults.clear();
    for(std::size_t i=0;i<17000;++i)driver->info.rawControls.push_back(std::to_string(i)+std::string(1000,'x'));
    const auto rig=engine.createRigSession(source,std::move(driver));const auto count=engine.objectCount();
    rejects([&]{(void)createObject3DRigMorphSamples(rig,{},std::array{RigMorphRequest{"sample",{}}});});
    CHECK(metrics->clones==0&&metrics->sourceCalls==0&&engine.objectCount()==count&&engine.rigCount()==1);
}
void formats(const std::filesystem::path &directory)
{
    Object3DEngine engine;const auto source=engine.createObject(fixture());auto metrics=std::make_shared<Metrics>();
    const auto rig=engine.createRigSession(source,std::make_unique<Driver>(metrics));
    const auto result=createObject3DRigMorphSamples(rig,input(RigControlSpace::Raw,.3),std::array{RigMorphRequest{"Quote\"Smile",input(RigControlSpace::Gui,.4)}});
    const auto manifest=exportRigSampleCatalogsJson(result.get().data().rigSampleCatalogs);CHECK(manifest.find("Quote\\\"Smile")!=std::string::npos);
    rejects([&]{(void)exportRigSampleCatalogsJson(result.get().data().rigSampleCatalogs,10);});
    auto editor=engine.editObject(result);editor.get().moveControlPoints(1,std::array{TopologyPointMove{0,{0,0,.001}}});CHECK(editor.get().undo());
    const auto bytes=exportObject3DProject(editor);CHECK(bytes.size()>24&&bytes[8]==14);
    Object3DEngine other;const auto loaded=importObject3DProject(other.reference(),bytes);CHECK(exportRigSampleCatalogsJson(loaded.editor.get().object().get().data().rigSampleCatalogs)==manifest);
    CHECK(loaded.editor.get().redo());CHECK(exportRigSampleCatalogsJson(loaded.editor.get().object().get().data().rigSampleCatalogs)==manifest);CHECK(loaded.editor.get().undo());
    PoseReference ordered;ordered.directions={{2,1,{0,1,0}}};ordered.order={{PoseReferenceStepKind::Direction,0}};
    editor.get().setReferencePose(ordered);const auto combined=exportObject3DProject(editor);CHECK(combined[8]==15);
    auto combinedLoaded=importObject3DProject(other.reference(),combined);
    CHECK(combinedLoaded.editor.get().object().get().data().referencePose==ordered);
    CHECK(exportRigSampleCatalogsJson(combinedLoaded.editor.get().object().get().data().rigSampleCatalogs)==manifest);
    auto names=engine.editObject(result);CHECK(names.get().renameJoints(std::array{JointRename{1,"link"},JointRename{2,"tip"}}));
    CHECK(exportRigSampleCatalogsJson(names.get().object().get().data().rigSampleCatalogs)==manifest);
    auto renamedLoaded=importObject3DProject(other.reference(),exportObject3DProject(names));
    CHECK(renamedLoaded.editor.get().object().get().data().bones[1].name=="link");
    CHECK(exportRigSampleCatalogsJson(renamedLoaded.editor.get().object().get().data().rigSampleCatalogs)==manifest);
    auto corrupt=bytes;corrupt.back()^=1;Object3DEngine invalid;rejects([&]{(void)importObject3DProject(invalid.reference(),corrupt);});CHECK(invalid.objectCount()==0);
    const auto glb=exportObject3DGlb(result.get());const auto fbx=exportObject3DFbx(result.get(),{.allowMorphNormalMetadata=true});
    Object3DEngine imported;const auto restored=importObject3DFbx(imported.reference(),fbx);CHECK(exportRigSampleCatalogsJson(restored.object.get().data().rigSampleCatalogs)==manifest);
    auto broken=fbx;constexpr std::array<std::uint8_t,8> signature{'i','i','A','O','R','S','M',0};
    const auto found=std::search(broken.begin(),broken.end(),signature.begin(),signature.end());CHECK(found!=broken.end()&&std::distance(found,broken.end())>24);
    *(found+24)^=1;Object3DEngine damaged;rejects([&]{(void)importObject3DFbx(damaged.reference(),broken);});CHECK(damaged.objectCount()==0);
    FbxImportOptions options;options.restoreAuthoringMetadata=false;const auto skipped=importObject3DFbx(imported.reference(),fbx,options);
    CHECK(skipped.object.get().data().rigSampleCatalogs.empty()&&!skipped.warnings.empty());
    std::filesystem::create_directories(directory);
    const auto write=[&](std::string_view name,const auto &v){std::ofstream f(directory/std::string(name),std::ios::binary);f.write(reinterpret_cast<const char*>(v.data()),v.size());CHECK(f.good());};
    write("samples.ii3dproject",bytes);write("samples.glb",glb);write("samples.fbx",fbx);write("samples.json",manifest);
}
void fbxFile(const std::filesystem::path &path)
{
    std::ifstream file(path,std::ios::binary);CHECK(file.good());const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
    Object3DEngine engine;(void)importObject3DFbx(engine.reference(),bytes);
}
void poseProject(const std::filesystem::path &directory)
{
    Object3DEngine engine;const auto source=engine.createObject(fixture());Object3DPoseProgram program;program.joints={"root","tip","link"};
    std::vector<Quaternion> deltas(3);deltas[1]=Quaternion::fromAxisAngle({0,1,0},.3);program.units.push_back({"Turn",std::move(deltas)});
    const auto rig=createObject3DPoseRig(source,program);const auto editor=engine.editObject(source);const auto bytes=exportObject3DProject(editor,std::array{rig});
    std::filesystem::create_directories(directory);std::ofstream out(directory/"source.ii3dproject",std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(out.good());
}
void lifetime()
{
    Object3DHandle result;
#if IIACTOROBJECT_HAS_NSCHARACTER
    std::shared_ptr<const NSCharacter::CharacterAsset> view;
#endif
    {
        Object3DEngine engine;const auto source=engine.createObject(fixture());const auto rig=engine.createRigSession(source,std::make_unique<Driver>(std::make_shared<Metrics>()));
        const auto requests=std::array{RigMorphRequest{"One",input(RigControlSpace::Raw,.8)}};
#if IIACTOROBJECT_HAS_NSCHARACTER
        NSCharacter::CharacterAsset character(source);view=NSCharacter::createCharacterRigMorphSamples(character,rig,{},requests);result=view->object();
        const auto other=engine.createObject(fixture());NSCharacter::CharacterAsset wrong(other);
        rejects([&]{(void)NSCharacter::createCharacterRigMorphSamples(wrong,rig,{},requests);});
#else
        result=createObject3DRigMorphSamples(rig,{},requests);
#endif
        CHECK(result.valid());
    }
    CHECK(!result.valid());
#if IIACTOROBJECT_HAS_NSCHARACTER
    CHECK(!view->valid());
#endif
}
struct HumanDriver final:Object3DRigDriver
{
    Object3DRigDefinition info;
    explicit HumanDriver(const std::string &bone)
    {info.sourceFormat="human-test";info.joints={bone};info.rawControls={"move"};info.drivenJoints=std::vector<std::size_t>{0};}
    const Object3DRigDefinition &definition()const noexcept override{return info;}
    std::unique_ptr<Object3DRigDriver> clone()const override{return std::make_unique<HumanDriver>(*this);}
    Object3DRigOutputs evaluate(std::span<const double> v,RigControlSpace,std::size_t)override
    {Object3DRigOutputs out;out.joints.resize(1);out.joints[0].translation.x=v[0];return out;}
};
void human(const std::filesystem::path &path)
{
    std::ifstream file(path,std::ios::binary);CHECK(file.good());const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
    Object3DEngine engine;const auto source=importObject3DFbx(engine.reference(),bytes).object;const auto &data=source.get().data();
    CHECK(data.bones.size()==163&&data.meshes.size()==5);std::size_t points=0;for(const auto &mesh:data.meshes)points+=mesh.topology?mesh.topology->points.size():mesh.geometry.vertices.size();
    CHECK(points==43474);const auto rig=engine.createRigSession(source,std::make_unique<HumanDriver>(data.bones[0].name));
    const RigSampleInput baseline{RigControlSpace::Raw,{{"move",0}}},target{RigControlSpace::Raw,{{"move",.002}}};
    const auto out=createObject3DRigMorphSamples(rig,baseline,std::array{RigMorphRequest{"BodyMove",target}});
    compare(rig,out,baseline,"",0);compare(rig,out,target,"BodyMove",0);
    for(std::size_t i=0;i<data.meshes.size();++i)
    {
        const auto &a=data.meshes[i],&b=out.get().data().meshes[i];CHECK(a.geometry.morphTargets.size()+1==b.geometry.morphTargets.size());
        if(a.topology)CHECK(b.topology&&a.topology->points.size()==b.topology->points.size());
    }
    std::cout<<"human bones="<<data.bones.size()<<" meshes="<<data.meshes.size()<<" sourcePoints="<<points<<'\n';
}
#if IIACTOROBJECT_HAS_DNA
void dna(const std::filesystem::path &directory)
{
    std::ifstream file(directory/"independent.dna",std::ios::binary);CHECK(file.good());const std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    Object3DEngine engine;const auto source=importObject3DDna(engine.reference(),std::as_bytes(std::span(bytes)));const auto original=source.rig.get().sourceBytes();
    for(const auto space:{RigControlSpace::Raw,RigControlSpace::Gui})for(std::size_t lod=0;lod<2;++lod)
    {
        RigSampleInput base{space,{{"smile",.25}}};const RigMorphRequest smile{"SampleSmile",{space,{{"smile",.5}}}};
        RigMorphSampleOptions options;options.lod=lod;const auto result=createObject3DRigMorphSamples(source.rig,base,std::array{smile},options);
        compare(source.rig,result,base,"",lod);compare(source.rig,result,smile.input,smile.morph,lod);
        const auto &catalog=result.get().data().rigSampleCatalogs[0];CHECK(std::abs(catalog.samples[0].state.animatedMaps[0]-(lod==0?.25:0))<1e-6);
        const auto editor=engine.editObject(result);const auto projectBytes=exportObject3DProject(editor,std::array{source.rig});Object3DEngine destination;
        const auto project=importObject3DProject(destination.reference(),projectBytes);CHECK(project.rigs.size()==1&&project.rigs[0].get().sourceBytes()==original);
        CHECK(exportRigSampleCatalogsJson(project.editor.get().object().get().data().rigSampleCatalogs)==exportRigSampleCatalogsJson(result.get().data().rigSampleCatalogs));
    }
    CHECK(source.rig.get().sourceBytes()==original);
}
#endif
int main(int argc,char **argv)
{
    try{if(argc<2)return 2;const std::string_view mode=argv[1];
        if(mode=="endpoints")endpoints();else if(mode=="partial")endpoints(true);else if(mode=="nonlinear")nonlinear();else if(mode=="failures")failures();else if(mode=="text_budget")textBudget();
        else if(mode=="lifetime")lifetime();else if(mode=="formats"&&argc==3)formats(argv[2]);else if(mode=="human"&&argc==3)human(argv[2]);
        else if(mode=="fbx_file"&&argc==3)fbxFile(argv[2]);else if(mode=="project_fixture"&&argc==3)poseProject(argv[2]);
#if IIACTOROBJECT_HAS_DNA
        else if(mode=="dna"&&argc==3)dna(argv[2]);
#endif
        else return 2;std::cout<<mode<<" passed\n";return 0;}
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
