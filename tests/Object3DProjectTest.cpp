#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DTemplateMorphs.h>
#include <iiActorObject/Object3DPoseRig.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
bool near(double a,double b){return std::abs(a-b)<1e-9;}
Object3DTemplateHandle model(Object3DEngine &engine)
{
    auto d=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 7 8 9\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nf 1/1 2/2 3/3 4/4\n");
    Object3DTemplateDefinition program;program.base=engine.createObject(std::move(d));
    program.targets={{"height",0,{{2,{0,.2,0}},{3,{0,.2,0}}}}};return engine.createTemplate(std::move(program));
}
void layers()
{
    std::vector<std::uint8_t> bytes;std::uint64_t revision=0;Object3DLayerId detail=0;
    {
        Object3DEngine source;auto editor=source.editTemplate(model(source),std::array{TemplateValue{"height",.5}});
        editor.get().moveControlPoints(0,std::array{TopologyPointMove{2,{0,0,.03}}});detail=editor.get().layers()[0].id;
        editor.get().renameLayer(detail,"Face detail");editor.get().setLayerWeight(detail,.5);
        TopologyMorph m;m.name="smile";m.normalDeltas=std::vector<TopologyNormalDelta>{{0,{.2,0,0}}};editor.get().setControlMorph(0,m);
        editor.get().setLayerEnabled(detail,false);CHECK(editor.get().undo());revision=editor.get().revision();
        bytes=exportObject3DProject(editor);CHECK(bytes==exportObject3DProject(editor));
    }
    Object3DEngine destination;auto project=importObject3DProject(destination.reference(),bytes);auto &e=project.editor.get();
    CHECK(e.layered()&&e.revision()==revision);CHECK(e.layers().size()==2&&e.layers()[0].id==detail);
    CHECK(e.layers()[0].label=="Face detail"&&near(e.layers()[0].weight,.5));
    const auto &t=*e.object().get().data().meshes[0].topology;CHECK(t.points.size()==5&&t.points[4].position==Vector3(7,8,9));
    CHECK(t.layout->definition().polygons[0].size()==4&&near(t.points[2].position.z,.015));
    CHECK(t.morphTargets[0].normalDeltas&&t.morphTargets[0].normalDeltas->size()==1);
    CHECK(e.redo()&&!e.layers()[0].enabled);CHECK(e.undo());
    e.setTemplateValues(std::array{TemplateValue{"height",1}});CHECK(!e.redo());
    CHECK(near(e.object().get().data().meshes[0].topology->points[2].position.y,1.2));
    CHECK(near(e.object().get().data().meshes[0].topology->points[2].position.z,.015));
    const auto last=e.layers().back().id;e.moveControlPoints(0,std::array{TopologyPointMove{0,{.01,0,0}}});CHECK(e.layers().back().id>last);
}
void snapshots()
{
    Object3DEngine engine;auto editor=engine.editTemplate(model(engine),{},2);editor.get().detachTemplate();
    editor.get().moveControlPoints(0,std::array{TopologyPointMove{0,{0,0,.02}}});CHECK(editor.get().undo());
    auto bytes=exportObject3DProject(editor);Object3DEngine other;auto project=importObject3DProject(other.reference(),bytes);
    CHECK(!project.editor.get().layered());CHECK(project.editor.get().redo());CHECK(project.editor.get().undo());CHECK(project.editor.get().undo());
    CHECK(project.editor.get().layered());CHECK(!project.editor.get().undo());
}
void materialSurface()
{
    Object3DEngine engine;auto editor=engine.editTemplate(model(engine));
    const Object3DMaterialSurface value{Object3DAlphaMode::Mask,.9,true};
    CHECK(editor.get().setMaterialSurface(0,value));CHECK(editor.get().layers().size()==1);
    const auto layer=editor.get().layers()[0].id;editor.get().setLayerWeight(layer,.5);
    auto current=editor.get().object().get().data().materials[0].surface;
    CHECK(current.alphaMode==Object3DAlphaMode::Mask&&current.doubleSided&&near(current.alphaCutoff,.7));
    editor.get().setTemplateValues(std::array{TemplateValue{"height",.5}});
    CHECK(near(editor.get().object().get().data().materials[0].surface.alphaCutoff,.7));
    const auto bytes=exportObject3DProject(editor);CHECK(bytes[8]==29);
    Object3DEngine destination;auto loaded=importObject3DProject(destination.reference(),bytes);
    auto &restored=loaded.editor.get();CHECK(restored.layered()&&restored.layers().size()==1);
    CHECK(near(restored.object().get().data().materials[0].surface.alphaCutoff,.7));
    restored.setLayerEnabled(layer,false);CHECK(restored.object().get().data().materials[0].surface==Object3DMaterialSurface{});
    CHECK(restored.undo());CHECK(restored.object().get().data().materials[0].surface.alphaMode==Object3DAlphaMode::Mask);
    CHECK(restored.redo());
    const auto before=editor.get().revision();
    rejects([&]{(void)editor.get().setMaterialSurface(0,{Object3DAlphaMode::Mask,-1,false});});
    CHECK(editor.get().revision()==before&&editor.get().layers().size()==1);
    editor.get().setLayerEnabled(layer,false);const auto disabled=exportObject3DProject(editor);CHECK(disabled[8]==29);
    Object3DEngine another;auto disabledProject=importObject3DProject(another.reference(),disabled);
    CHECK(disabledProject.editor.get().object().get().data().materials[0].surface==Object3DMaterialSurface{});
    CHECK(disabledProject.editor.get().undo());
    CHECK(disabledProject.editor.get().object().get().data().materials[0].surface.alphaMode==Object3DAlphaMode::Mask);
}
void limits()
{
    Object3DEngine source;auto editor=source.editTemplate(model(source));editor.get().moveControlPoints(0,std::array{TopologyPointMove{0,{0,0,.02}}});
    const auto bytes=exportObject3DProject(editor);Object3DEngine target({1,1,1,1});auto original=target.createObject(editor.get().object().get().data());
    const auto oldId=original.id();rejects([&]{(void)importObject3DProject(target.reference(),bytes);});
    CHECK(original.valid()&&original.id()==oldId&&target.objectCount()==1&&target.editorCount()==0&&target.templateCount()==0);
    target.eraseObject(oldId);auto next=target.createObject(source.createObject(editor.get().object().get().data()).get().data());CHECK(next.id()==oldId+1);
    Object3DProjectOptions options;options.maximumBytes=32;rejects([&]{(void)exportObject3DProject(editor,{},options);});
    Object3DEngine room;options={};options.maximumDecodedBytes=1;rejects([&]{(void)importObject3DProject(room.reference(),bytes,options);});CHECK(room.objectCount()==0);
}
void corruption()
{
    Object3DEngine source;auto editor=source.editTemplate(model(source));const auto bytes=exportObject3DProject(editor);
    Object3DEngine target;for(const auto n:{std::size_t{0},std::size_t{12},bytes.size()-1})rejects([&]{(void)importObject3DProject(target.reference(),std::span(bytes).first(n));});
    auto bad=bytes;bad.back()^=1;rejects([&]{(void)importObject3DProject(target.reference(),bad);});
    bad=bytes;bad.push_back(0);rejects([&]{(void)importObject3DProject(target.reference(),bad);});CHECK(target.objectCount()==0&&target.editorCount()==0);
}
void triangulation()
{
    Object3DEngine source;auto object=model(source).get().generate();auto editor=source.editObject(object);
    editor.get().moveControlPoints(0,std::array{TopologyPointMove{0,{.8,.8,0}}});
    const auto &mesh=editor.get().object().get().data().meshes[0];const auto &t=*mesh.topology;
    auto retriangulated=MeshTopologyLayout::create(t.layout->definition(),t.points);
    CHECK(!std::equal(t.layout->triangles().begin(),t.layout->triangles().end(),retriangulated->triangles().begin()));
    Object3DEngine target;auto loaded=importObject3DProject(target.reference(),exportObject3DProject(editor));
    CHECK(loaded.editor.get().object().get().data().meshes[0].geometry.triangles==mesh.geometry.triangles);
}
void atomic()
{
    Object3DEngine source;auto original=source.editTemplate(model(source));original.get().moveControlPoints(0,std::array{TopologyPointMove{0,{0,0,.02}}});
    const auto bytes=exportObject3DProject(original);Object3DEngine target({64,1,1,2});auto object=target.createObject(original.get().object().get().data());
    auto existing=target.editObject(object);const auto revision=existing.get().revision();
    rejects([&]{(void)importObject3DProject(target.reference(),bytes);});
    CHECK(object.valid()&&existing.valid()&&target.objectCount()==1&&target.editorCount()==1&&target.templateCount()==0);
    CHECK(existing.get().revision()==revision);auto next=target.createObject(object.get().data());CHECK(next.id()==object.id()+1);
    Object3DEditorHandle borrowed;{Object3DEngine owner;borrowed=importObject3DProject(owner.reference(),bytes).editor;CHECK(borrowed.valid());}
    CHECK(!borrowed.valid());rejects([&]{(void)borrowed.get();});
}
void restoreValidation()
{
    Object3DEngine engine;auto base=model(engine).get().generate();const auto &t=*base.get().data().meshes[0].topology;
    const auto definition=t.layout->definition();const std::vector<std::uint32_t> triangles(t.layout->triangles().begin(),t.layout->triangles().end());
    const std::vector<std::uint32_t> faces(t.layout->trianglePolygons().begin(),t.layout->trianglePolygons().end());
    auto valid=MeshTopologyLayout::restore(definition,t.points,triangles,faces);CHECK(valid->definition()==definition);
    auto bad=triangles;std::swap(bad[1],bad[2]);rejects([&]{(void)MeshTopologyLayout::restore(definition,t.points,bad,faces);});
    bad=triangles;bad[0]=999;rejects([&]{(void)MeshTopologyLayout::restore(definition,t.points,bad,faces);});
    bad=triangles;std::copy_n(bad.begin(),3,bad.begin()+3);rejects([&]{(void)MeshTopologyLayout::restore(definition,t.points,bad,faces);});
    auto hidden=definition;hidden.hiddenPolygons={0};rejects([&]{(void)MeshTopologyLayout::restore(hidden,t.points,triangles,faces);});
}
void runtime()
{
    Object3DEngine source;auto d=model(source).get().generate().get().data();d.meshes[0].topology.reset();
    d.meshes[0].geometry.morphTargets={{"normal",{{0,{0,0,.01},{.2,0,0}}}}};d.materials[0].roughness=.15;d.materials[0].baseColor[3]=.4;
    auto editor=source.editObject(source.createObject(std::move(d)));Object3DEngine target;
    auto project=importObject3DProject(target.reference(),exportObject3DProject(editor));const auto &m=project.editor.get().object().get().data().meshes[0];
    CHECK(!m.topology&&m.geometry.morphTargets[0].deltas[0].normal==Vector3(.2,0,0));
    CHECK(near(project.editor.get().object().get().data().materials[0].roughness,.15));
}
void customDriver()
{
    struct Driver final:Object3DRigDriver
    {
        Object3DRigDefinition info;
        explicit Driver(std::string joint){info.sourceFormat="dna";info.joints={std::move(joint)};}
        const Object3DRigDefinition &definition() const noexcept override{return info;}
        std::unique_ptr<Object3DRigDriver> clone() const override{return std::make_unique<Driver>(*this);}
        Object3DRigOutputs evaluate(std::span<const double>,RigControlSpace,std::size_t)override{Object3DRigOutputs out;out.joints.resize(1);return out;}
    };
    Object3DEngine engine;auto editor=engine.editTemplate(model(engine));
    auto driver=std::make_unique<Driver>(editor.get().object().get().data().bones[0].name);
    auto rig=engine.createRigSession(editor.get().object(),std::move(driver));
    rejects([&]{(void)exportObject3DProject(editor,std::array{rig});});CHECK(rig.valid()&&editor.valid());
    auto impostor=std::make_unique<Driver>(editor.get().object().get().data().bones[0].name);
    impostor->info.sourceFormat="iiActorObject.templateMorphRig.1";
    const auto falseRig=engine.createRigSession(editor.get().object(),std::move(impostor));
    rejects([&]{(void)exportObject3DProject(editor,std::array{falseRig});});
    rejects([&]{(void)inspectObject3DTemplateMorphRig(falseRig);});
}
std::vector<std::uint8_t> morphProject(bool mixed)
{
    Object3DEngine engine;const auto original=model(engine);auto definition=original.get().definition();
    auto data=definition.base.get().data();auto garment=data.meshes[0];garment.name="Garment";
    for(auto &point:garment.topology->points)point.position.z+=.04;
    data.meshes.push_back(std::move(garment));
    data.meshes[0].topology->morphTargets.push_back({"rig_height_control",{{0,{.1,0,0}}}});
    data.kind=Object3DKind::SkeletalMesh;data.bones.push_back({"child",0,{}});
    definition.joints={{0,{0,{0}},{0,{2}},{}},{1,{1,{1}},{1,{2}},{}}};bindObject3DTemplateJoints(data,definition.joints);
    definition.base=engine.createObject(data);
    std::array<SurfaceControlPoint,4> mapping{};
    for(std::uint32_t i=0;i<mapping.size();++i)mapping[i]={i,{i,i,i},{1,0,0},{0,0,.04}};
    definition.surfaces={bindMeshControlPoints(data,0,1,mapping)};
    // This displacement is deliberately below the saved threshold. Re-baking
    // with default epsilon would reintroduce a different source point.
    definition.targets[0].deltas.push_back({0,{0,0,1e-13}});
    definition.targets.push_back({"body_width",0,{{1,{.1,0,0}},{2,{.1,0,0}}},0,1});
    const auto program=engine.createTemplate(std::move(definition));
    const std::array baseline{TemplateValue{"height",.25},TemplateValue{"body_width",.2}};
    const std::array samples{TemplateMorphSample{"height",.8,"rig_height_control"},
                             TemplateMorphSample{"height",0,"rig_height_down"}};
    TemplateMorphBakeOptions settings;settings.replaceExisting=true;settings.epsilon=1e-12;settings.maximumSamples=8;settings.maximumPointVisits=200'000;settings.maximumDeltas=4096;
    const auto baked=bakeObject3DTemplateMorphs(program,baseline,samples,settings);
    const auto &filtered=baked.object.get().data().meshes[0].topology->morphTargets;
    const auto match=[](const auto &m){return m.name=="rig_height_control";};
    const auto chosen=std::find_if(filtered.begin(),filtered.end(),match);CHECK(chosen!=filtered.end());
    CHECK(std::none_of(chosen->deltas.begin(),chosen->deltas.end(),[](const auto &v){return v.point==0;}));
    auto unfilteredSettings=settings;unfilteredSettings.epsilon=0;
    const auto unfiltered=bakeObject3DTemplateMorphs(program,baseline,samples,unfilteredSettings);
    const auto &all=unfiltered.object.get().data().meshes[0].topology->morphTargets;
    const auto unfilteredMorph=std::find_if(all.begin(),all.end(),match);CHECK(unfilteredMorph!=all.end());
    CHECK(std::any_of(unfilteredMorph->deltas.begin(),unfilteredMorph->deltas.end(),[](const auto &v){return v.point==0;}));
    CHECK(baked.templateProgram.valid()&&baked.templateProgram.id()==program.id()&&baked.baselineInputs&&*baked.baselineInputs==std::vector<TemplateValue>(baseline.begin(),baseline.end()));
    const auto rig=createObject3DTemplateMorphRig(baked);const auto info=inspectObject3DTemplateMorphRig(rig);
    CHECK(info.templateProgram.valid()&&info.templateProgram.id()==program.id()&&info.program.baselineInputs==baked.baselineInputs&&info.program.channels.size()==2);
    CHECK(info.program.bakeOptions&&*info.program.bakeOptions==settings);
    CHECK(info.program.channels[0].input=="height"&&info.program.channels[0].baseline==.25&&info.program.channels[0].endpoint==.8);
    CHECK(info.program.channels[0].meshes==std::vector<std::string>({"Garment","OBJ"}));
    CHECK(info.program.channels[0].changedJoints==std::vector<std::string>({"root","child"}));
    CHECK(info.program.channels[0].changedJointIds==std::vector<BoneIndex>({0,1}));
    auto editor=engine.editObject(baked.object);editor.get().moveControlPoints(0,std::array{TopologyPointMove{0,{0,0,.01}}});
    std::vector<Object3DRigHandle> rigs{rig};
    if(mixed)
    {
        const auto pose=createObject3DPoseRig(baked.object,{{"root"},{{"turn",{Quaternion::fromAxisAngle({0,1,0},.2)}}}});
        rigs.push_back(pose);rigs.push_back(rig.get().cloneFor(baked.object));
    }
    auto bytes=exportObject3DProject(editor,rigs);CHECK(bytes.size()>24&&bytes[8]==21);
    CHECK(bytes==exportObject3DProject(editor,rigs));
#if IIACTOROBJECT_HAS_NSCHARACTER
    NSCharacter::CharacterEditor view(editor);CHECK(NSCharacter::exportProject(view,rigs)==bytes);
    Object3DEngine characterEngine;const auto character=NSCharacter::importProject(characterEngine.reference(),bytes);
    CHECK(character.rigs.size()==rigs.size()&&inspectObject3DTemplateMorphRig(character.rigs[0]).templateProgram.valid());
#endif
    return bytes;
}
void morphRigProject()
{
    const auto bytes=morphProject(true);Object3DEngine engine;const auto restored=importObject3DProject(engine.reference(),bytes);
    CHECK(restored.rigs.size()==3&&restored.editor.get().undo()&&restored.editor.get().redo());
    const auto info=inspectObject3DTemplateMorphRig(restored.rigs[0]);
    CHECK(info.templateProgram.valid()&&info.program.baselineInputs&&info.program.baselineInputs->size()==2);
    CHECK(info.program.channels.size()==2&&info.program.channels[0].name=="rig_height_control");
    CHECK(info.program.bakeOptions&&info.program.bakeOptions->replaceExisting&&info.program.bakeOptions->epsilon==1e-12&&info.program.bakeOptions->maximumSamples==8);
    CHECK(info.program.channels[0].changedJoints==std::vector<std::string>({"root","child"}));
    CHECK(info.program.channels[0].changedJointIds==std::vector<BoneIndex>({0,1}));
    CHECK(inspectObject3DTemplateMorphRig(restored.rigs[2]).program==info.program);
    rejects([&]{(void)inspectObject3DTemplateMorphRig(restored.rigs[1]);});
    auto first=restored.rigs[0].get().object().get().instantiate();
    auto second=restored.rigs[2].get().object().get().instantiate();
    restored.rigs[0].get().applyTo(*first,restored.rigs[0].get().evaluate(std::array{.3,.4}));
    restored.rigs[2].get().applyTo(*second,restored.rigs[2].get().evaluate(std::array{.7,.2}));
    applyObject3DTemplateMorphRigWithBind(restored.rigs[0],*first,std::array{NamedRigControl{"rig_height_control",.3},NamedRigControl{"rig_height_down",.4}});
    applyObject3DTemplateMorphRigWithBind(restored.rigs[2],*second,std::array{NamedRigControl{"rig_height_control",.7},NamedRigControl{"rig_height_down",.2}});
    CHECK(!first->pose().deformationBindTransforms().empty()&&!second->pose().deformationBindTransforms().empty());
    CHECK(first->pose().deformationBindTransforms()[0]!=second->pose().deformationBindTransforms()[0]);
    for(auto *component:first->components())if(auto *mesh=dynamic_cast<SkeletalMeshComponent *>(component))
        CHECK(mesh->morphWeights()[0]==.3&&mesh->morphWeights()[1]==.4);
    for(auto *component:second->components())if(auto *mesh=dynamic_cast<SkeletalMeshComponent *>(component))
        CHECK(mesh->morphWeights()[0]==.7&&mesh->morphWeights()[1]==.2);
    std::vector<TemplateMorphSample> samples;
    for(const auto &channel:info.program.channels)samples.push_back({channel.input,channel.endpoint,channel.name});
    const auto regenerated=bakeObject3DTemplateMorphs(info.templateProgram,*info.program.baselineInputs,samples,*info.program.bakeOptions);
    const auto &original=restored.rigs[0].get().object().get().data(),&againData=regenerated.object.get().data();
    CHECK(original.meshes.size()==againData.meshes.size());
    for(std::size_t mesh=0;mesh<original.meshes.size();++mesh)
    {
        const auto &a=*original.meshes[mesh].topology,&b=*againData.meshes[mesh].topology;
        CHECK(a.points.size()==b.points.size()&&a.morphTargets.size()==b.morphTargets.size());
        CHECK(a.layout->definition()==b.layout->definition()&&a.textureCoordinates==b.textureCoordinates);
        CHECK(a.layout->triangles().size()==b.layout->triangles().size()&&
              std::equal(a.layout->triangles().begin(),a.layout->triangles().end(),b.layout->triangles().begin()));
        for(std::size_t i=0;i<a.points.size();++i)
        {
            CHECK(near(a.points[i].position.x,b.points[i].position.x)&&near(a.points[i].position.y,b.points[i].position.y)&&near(a.points[i].position.z,b.points[i].position.z));
            CHECK(a.points[i].weights.size()==b.points[i].weights.size());
            for(std::size_t w=0;w<a.points[i].weights.size();++w)
                CHECK(a.points[i].weights[w].bone==b.points[i].weights[w].bone&&near(a.points[i].weights[w].weight,b.points[i].weights[w].weight));
        }
        for(std::size_t i=0;i<a.morphTargets.size();++i)
        {
            CHECK(a.morphTargets[i].name==b.morphTargets[i].name&&a.morphTargets[i].deltas.size()==b.morphTargets[i].deltas.size());
            CHECK(a.morphTargets[i].sourceTarget==b.morphTargets[i].sourceTarget&&a.morphTargets[i].normalDeltas==b.morphTargets[i].normalDeltas);
            for(std::size_t j=0;j<a.morphTargets[i].deltas.size();++j)
            {
                const auto &x=a.morphTargets[i].deltas[j],&y=b.morphTargets[i].deltas[j];
                CHECK(x.point==y.point&&near(x.position.x,y.position.x)&&near(x.position.y,y.position.y)&&near(x.position.z,y.position.z));
            }
        }
    }
    auto values=*info.program.baselineInputs;for(auto &value:values)if(value.name=="height")value.value=.8;
    const auto expected=info.templateProgram.get().snapshot(values);auto actor=restored.rigs[0].get().object().get().instantiate();
    const std::array named{NamedRigControl{"rig_height_control",1}};
    applyObject3DTemplateMorphRigWithBind(restored.rigs[0],*actor,named);
    auto targetSkeleton=std::make_shared<Skeleton>(expected.bones);const Pose targetPose(targetSkeleton);
    for(std::size_t joint=0;joint<expected.bones.size();++joint)
    {
        const auto a=actor->pose().worldMatrices()[joint].transformPoint({});
        const auto b=targetPose.worldMatrices()[joint].transformPoint({});
        CHECK(near(a.x,b.x)&&near(a.y,b.y)&&near(a.z,b.z));
    }
    std::size_t mesh=0;
    for(auto *component:actor->components())if(auto *part=dynamic_cast<SkeletalMeshComponent *>(component))
    {
        CHECK(mesh<expected.meshes.size());const auto actual=part->deformedVertices();
        CHECK(actual.size()==expected.meshes[mesh].geometry.vertices.size());
        for(std::size_t i=0;i<actual.size();++i)
        {
            const auto a=actual[i].position,b=expected.meshes[mesh].geometry.vertices[i].position;
            CHECK(near(a.x,b.x)&&near(a.y,b.y)&&near(a.z,b.z));
        }
        ++mesh;
    }
    CHECK(mesh==2);const auto again=exportObject3DProject(restored.editor,restored.rigs);
    CHECK(again==exportObject3DProject(restored.editor,restored.rigs));
    auto renamed=restored.rigs[0].get().object().get().data();renamed.bones[1].name="child_renamed";
    const auto renamedObject=engine.createObject(std::move(renamed));
    const auto rebound=restored.rigs[0].get().cloneFor(renamedObject);
    CHECK(inspectObject3DTemplateMorphRig(rebound).program==info.program);
    auto renamedActor=renamedObject.get().instantiate();const auto renamedRevision=renamedActor->pose().revision();
    rejects([&]{applyObject3DTemplateMorphRigWithBind(rebound,*renamedActor,named);});
    CHECK(renamedActor->pose().revision()==renamedRevision);
    const auto renamedBytes=exportObject3DProject(restored.editor,std::array{rebound});Object3DEngine renamedEngine;
    const auto reloaded=importObject3DProject(renamedEngine.reference(),renamedBytes);
    CHECK(inspectObject3DTemplateMorphRig(reloaded.rigs[0]).program==info.program);
    CHECK(reloaded.rigs[0].get().object().get().data().bones[1].name=="child_renamed");
    Object3DEngine limited(Object3DEngineLimits{.rigs=1});const auto survivor=limited.createObject(parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));
    const auto occupied=createObject3DPoseRig(survivor,{{"root"},{{"turn",{Quaternion::fromAxisAngle({0,1,0},.1)}}}});
    const auto id=survivor.id();rejects([&]{(void)importObject3DProject(limited.reference(),bytes);});
    CHECK(survivor.valid()&&occupied.valid()&&limited.objectCount()==1&&limited.editorCount()==0&&limited.templateCount()==0&&limited.rigCount()==1);
    const auto next=limited.createObject(survivor.get().data());CHECK(next.id()==id+1);
}
void morphRigMissingSource()
{
    Object3DEngine engine;const auto program=model(engine);
    const std::array baseline{TemplateValue{"height",.25}};
    const std::array sample{TemplateMorphSample{"height",.8,"height_saved"}};
    const auto baked=bakeObject3DTemplateMorphs(program,baseline,sample);
    const auto rig=createObject3DTemplateMorphRig(baked);auto editor=engine.editObject(baked.object);
    CHECK(engine.eraseTemplate(program.id())&&!inspectObject3DTemplateMorphRig(rig).templateProgram.valid());
    auto unknown=baked;unknown.baselineInputs.reset();unknown.bakeOptions.reset();unknown.templateProgram={};
    const auto unknownRig=createObject3DTemplateMorphRig(unknown);
    const std::array rigs{rig,unknownRig};const auto bytes=exportObject3DProject(editor,rigs);
    Object3DEngine destination;const auto restored=importObject3DProject(destination.reference(),bytes);
    CHECK(restored.rigs.size()==2);
    const auto known=inspectObject3DTemplateMorphRig(restored.rigs[0]);
    const auto missing=inspectObject3DTemplateMorphRig(restored.rigs[1]);
    CHECK(!known.templateProgram.valid()&&known.program.baselineInputs&&known.program.baselineInputs->size()==1&&known.program.bakeOptions);
    CHECK(!missing.templateProgram.valid()&&!missing.program.baselineInputs&&!missing.program.bakeOptions);
    auto actor=restored.rigs[0].get().object().get().instantiate();
    restored.rigs[0].get().applyTo(*actor,restored.rigs[0].get().evaluate(std::array{1.0}));
    std::size_t meshes=0;for(auto *component:actor->components())if(auto *mesh=dynamic_cast<SkeletalMeshComponent *>(component))
    {CHECK(mesh->morphWeights().size()==1&&mesh->morphWeights()[0]==1);++meshes;}
    CHECK(meshes==1);
}
void morphRigLimits()
{
    Object3DEngine engine(Object3DEngineLimits{.rigs=300});const auto program=model(engine);
    const std::array sample{TemplateMorphSample{"height",.8,"height_saved"}};
    const auto baked=bakeObject3DTemplateMorphs(program,{},sample);const auto rig=createObject3DTemplateMorphRig(baked);
    auto editor=engine.editObject(baked.object);std::vector<Object3DRigHandle> rigs{rig};
    for(std::size_t i=1;i<257;++i)rigs.push_back(rig.get().cloneFor(baked.object));
    const auto count=engine.rigCount();rejects([&]{(void)exportObject3DProject(editor,rigs);});
    CHECK(engine.rigCount()==count&&rig.valid()&&editor.valid());
}
void morphProjectFixture(const std::filesystem::path &path)
{
    const auto bytes=morphProject(false);std::filesystem::create_directories(path.parent_path());
    std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));CHECK(f.good());
}
void defaultMorphProjectFixture(const std::filesystem::path &path)
{
    Object3DEngine engine;const auto object=engine.createObject(parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));
    auto editor=engine.editObject(object);editor.get().setControlMorph(0,{"DefaultFixtureMorph",{{0,{0,.1,0}}}});
    editor.get().setDefaultMorphWeight(0,"DefaultFixtureMorph",-.375);
    const auto bytes=exportObject3DProject(editor);CHECK(bytes.size()>24&&bytes[8]==23);
    std::filesystem::create_directories(path.parent_path());std::ofstream f(path,std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));CHECK(f.good());
}
void projectFile(const std::filesystem::path &path,bool write)
{
    if(write)
    {
        Object3DEngine engine;auto editor=engine.editTemplate(model(engine));editor.get().moveControlPoints(0,std::array{TopologyPointMove{0,{0,0,.02}}});
        auto bytes=exportObject3DProject(editor);std::filesystem::create_directories(path.parent_path());std::ofstream file(path,std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(file.good());return;
    }
    std::ifstream file(path,std::ios::binary);CHECK(file.good());std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
    Object3DEngine engine;auto object=engine.createObject(parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));
    try{(void)importObject3DProject(engine.reference(),bytes);}
    catch(...)
    {
        CHECK(object.valid()&&engine.objectCount()==1&&engine.editorCount()==0&&engine.rigCount()==0&&engine.templateCount()==0);
        const auto next=engine.createObject(object.get().data());CHECK(next.id()==object.id()+1);throw;
    }
}
void legacy(const std::filesystem::path &path)
{
    std::ifstream file(path,std::ios::binary);CHECK(file.good());std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
    CHECK(bytes.size()>24&&bytes[8]>=1&&bytes[8]<=8);Object3DEngine engine;auto project=importObject3DProject(engine.reference(),bytes);
    CHECK(project.editor.get().layered()&&project.editor.get().layers().size()==1);
    CHECK(!project.editor.get().templateProgram().get().definition().grounding.has_value());
    if(bytes[8]==3){const auto masks=project.editor.get().layerPointMasks(project.editor.get().layers()[0].id);CHECK(masks.size()==1&&masks[0].weights==std::vector<double>({1,.5,.25,0,.5}));}
    project.editor.get().setTemplateValues(std::array{TemplateValue{"height",1}});
    const auto &t=*project.editor.get().object().get().data().meshes[0].topology;CHECK(near(t.points[0].position.z,.02)&&near(t.points[2].position.y,1.2));
}
#if IIACTOROBJECT_HAS_NSCHARACTER
void human(const std::filesystem::path &directory,const std::filesystem::path &output)
{
    std::vector<std::uint8_t> bytes;Vector3 expected;std::uint32_t point=0;std::string unused;
    {
        Object3DEngine engine;auto human=NSCharacter::loadMakeHumanTemplate(engine.reference(),directory);auto editor=human.edit({},8);
        unused=human.program().get().definition().targets.back().name;
        const auto &t=*editor.asset()->data().meshes[0].topology;point=t.layout->definition().layouts[t.layout->definition().renderToLayout[0]].position;
        editor.moveControlPoints(0,std::array{TopologyPointMove{point,{0,0,.003}}});editor.setTemplateValues(std::array{TemplateValue{"nose/nose-scale-vert-incr",.4}});
        expected=editor.asset()->data().meshes[0].topology->points[point].position;bytes=NSCharacter::exportProject(editor);
    }
    Object3DEngine engine;auto project=NSCharacter::importProject(engine.reference(),bytes);auto &editor=project.editor;
    const auto &d=editor.asset()->data();CHECK(d.bones.size()==163&&d.meshes[0].topology->points.size()==19158);
    CHECK(d.meshes[0].topology->layout->definition().polygons.size()==18486);CHECK(d.meshes[0].topology->points[point].position==expected);
    const auto layout=d.meshes[0].topology->layout;CHECK(editor.undo());CHECK(editor.asset()->data().meshes[0].topology->layout==layout);CHECK(editor.redo());
    editor.setTemplateValues(std::array{TemplateValue{unused,.25}});CHECK(editor.layers().size()==1);
    std::filesystem::create_directories(output);std::ofstream f(output/"Human.ii3dproject",std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(f.good());
    std::cout<<"projectBytes="<<bytes.size()<<" sourcePoints=19158 bones=163 sharedLayout=1 targetCatalog=472\n";
}
#endif
int main(int argc,char**argv)
{
    try{if(argc<2)return 2;const std::string_view mode=argv[1];if(mode=="layers")layers();else if(mode=="snapshots")snapshots();else if(mode=="material_surface")materialSurface();else if(mode=="limits")limits();else if(mode=="corruption")corruption();else if(mode=="triangulation")triangulation();else if(mode=="atomic")atomic();else if(mode=="restore_validation")restoreValidation();else if(mode=="runtime")runtime();else if(mode=="custom_driver")customDriver();else if(mode=="morph_rig")morphRigProject();else if(mode=="morph_missing_source")morphRigMissingSource();else if(mode=="morph_limits")morphRigLimits();else if(mode=="morph_fixture"&&argc==3)morphProjectFixture(argv[2]);else if(mode=="default_morph_fixture"&&argc==3)defaultMorphProjectFixture(argv[2]);else if(mode=="legacy"&&argc==3)legacy(argv[2]);else if((mode=="fixture"||mode=="file")&&argc==3)projectFile(argv[2],mode=="fixture");
#if IIACTOROBJECT_HAS_NSCHARACTER
    else if(mode=="human"&&argc==4)human(argv[2],argv[3]);
#endif
    else return 2;std::cout<<mode<<" passed\n";return 0;}
    catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
