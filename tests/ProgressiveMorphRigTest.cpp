#include <iiActorObject/Object3DProgressiveMorph.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#endif
#include "../src/third_party/ufbx/ufbx.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string_view>

using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
bool close(double a,double b){return std::abs(a-b)<1e-10*std::max({1.,std::abs(a),std::abs(b)});}
Object3DData data()
{
    Object3DData d;d.bones={{"root",NoBone,{}},{"jaw",0,{{0,1,0}}}};
    Object3DMesh mesh;mesh.name="Face";
    mesh.geometry.vertices={{{0,0,0},{0,0,1},{{0,1}}},{{1,0,0},{0,0,1},{{0,1}}},{{0,1,0},{0,0,1},{{0,1}}}};
    mesh.geometry.triangles={0,1,2};
    for(const auto &[name,dy]:std::array{std::pair{"NegativeFar",-.6},std::pair{"NegativeNear",-.15},
        std::pair{"PositiveNear",.25},std::pair{"PositiveMid",.8},std::pair{"PositiveFar",1.2},std::pair{"Blink",.4}})
        mesh.geometry.morphTargets.push_back({name,{{0,{0,dy,0},{}}}});
    d.meshes.push_back(std::move(mesh));return d;
}
ProgressiveMorphProgram program()
{
    return {{{"Expression",.15,{{"NegativeFar",-.75},{"NegativeNear",-.2},
        {"PositiveNear",.3},{"PositiveMid",.8},{"PositiveFar",1}}},
        {"Blink",0,{{"Blink",1}}}}};
}
double output(const Object3DRigSession &rig,const Object3DRigEvaluation &value,std::string_view name)
{
    const auto &names=rig.definition().blendShapes;const auto found=std::find(names.begin(),names.end(),name);
    CHECK(found!=names.end());return value.blendShapes[static_cast<std::size_t>(found-names.begin())];
}
void curve()
{
    Object3DEngine engine;const auto object=engine.createObject(data());const auto rig=createObject3DProgressiveMorphRig(object,program());
    CHECK(inspectObject3DProgressiveMorphRig(rig)==program());
    const auto value=rig.get().evaluateNamed(std::array{NamedRigControl{"Expression",.55},NamedRigControl{"Blink",.2}});
    CHECK(close(output(rig.get(),value,"PositiveNear"),.5)&&close(output(rig.get(),value,"PositiveMid"),.5));
    CHECK(close(output(rig.get(),value,"PositiveFar"),0)&&close(output(rig.get(),value,"Blink"),.2));
    auto actor=object.get().instantiate();auto jaw=actor->pose().locals()[1];jaw.position.x=.4;actor->pose().setLocal(1,jaw);
    rig.get().applyTo(*actor,value);CHECK(actor->pose().locals()[1]==jaw);
    auto *mesh=actor->findComponent<SkeletalMeshComponent>();CHECK(mesh);
    CHECK(close(mesh->deformedVertices()[0].position.y,.5*.25+.5*.8+.2*.4));
    auto dual=data();dual.meshes[0].geometry.skinning.method=SkinningMethod::DualQuaternion;
    const auto dualObject=engine.createObject(std::move(dual));const auto dualRig=createObject3DProgressiveMorphRig(dualObject,program());
    auto dualActor=dualObject.get().instantiate();dualRig.get().applyTo(*dualActor,dualRig.get().evaluateNamed(
        std::array{NamedRigControl{"Expression",.55},NamedRigControl{"Blink",.2}}));
    CHECK(close(dualActor->findComponent<SkeletalMeshComponent>()->deformedVertices()[0].position.y,.605));
    const auto defaults=rig.get().evaluateNamed({});
    CHECK(close(output(rig.get(),defaults,"PositiveNear"),.5));
    CHECK(rig.get().evaluateNamed(std::array{NamedRigControl{"Expression",.55}},RigControlSpace::Gui).blendShapes==
          rig.get().evaluateNamed(std::array{NamedRigControl{"Expression",.55}}).blendShapes);
    const AnimationClip animation(object.get().skeleton(),1,{},{{"Expression",{{0,0},{1,1}}}});
    const auto animated=rig.get().evaluateNamed(std::array{NamedRigControl{"Expression",animation.sampleCurve(0,.55)}});
    CHECK(close(output(rig.get(),animated,"PositiveNear"),.5));
    auto changed=data();changed.meshes[0].geometry.morphTargets[2].deltas[0].position.y=.5;
    const auto edited=engine.createObject(changed);const auto rebound=rig.get().cloneFor(edited);
    auto editedActor=edited.get().instantiate();rebound.get().applyTo(*editedActor,rebound.get().evaluateNamed(std::array{NamedRigControl{"Expression",.3}}));
    CHECK(close(editedActor->findComponent<SkeletalMeshComponent>()->deformedVertices()[0].position.y,.5));
    changed.meshes[0].geometry.morphTargets.erase(changed.meshes[0].geometry.morphTargets.begin());
    const auto invalid=engine.createObject(changed);const auto count=engine.rigCount();
    rejects([&]{(void)rig.get().cloneFor(invalid);});CHECK(engine.rigCount()==count);
}
void split()
{
    auto input=data();auto second=input.meshes.front();second.name="Face2";
    second.geometry.vertices[0].position.z=.3;
    second.geometry.vertices[0].weights={{1,1}};
    second.geometry.morphTargets[2].deltas[0].position.y=.9;
    second.geometry.morphTargets.pop_back();
    auto third=input.meshes.front();third.name="Face3";
    third.geometry.vertices[0].position.z=.6;
    third.geometry.morphTargets.erase(third.geometry.morphTargets.begin(),third.geometry.morphTargets.end()-1);
    third.geometry.morphTargets.push_back({"Other",{{0,{0,.2,0},{}}}});
    input.meshes.push_back(std::move(second));input.meshes.push_back(std::move(third));
    auto p=program();p.channels[0].defaultValue=0;
    Object3DEngine engine;const auto object=engine.createObject(std::move(input));
    const auto rig=createObject3DProgressiveMorphRig(object,p);
    const auto bytes=exportObject3DFbxWithProgressiveMorphs(rig);
    ufbx_error error{};ufbx_load_opts strict{};strict.file_format=UFBX_FILE_FORMAT_FBX;strict.strict=true;
    using Scene=std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)>;
    Scene scene(ufbx_load_memory(bytes.bytes.data(),bytes.bytes.size(),&strict,&error),ufbx_free_scene);
    CHECK(scene&&scene->meshes.count==3&&scene->skin_deformers.count==3);
    std::vector<ufbx_prop_override_desc> overrides;
    for(const auto *channel:scene->blend_channels)
    {
        const std::string_view name(channel->name.data,channel->name.length);
        if(name!="Expression"&&name!="Blink")continue;
        ufbx_prop_override_desc value{};value.element_id=channel->element_id;
        value.prop_name={"DeformPercent",13};value.value.x=name=="Expression"?55:20;
        overrides.push_back(value);
    }
    CHECK(overrides.size()==4);
    ufbx_anim_opts animationOptions{};animationOptions.prop_overrides={overrides.data(),overrides.size()};
    std::unique_ptr<ufbx_anim,decltype(&ufbx_free_anim)> animation(ufbx_create_anim(scene.get(),&animationOptions,&error),ufbx_free_anim);
    CHECK(animation);Scene sample(ufbx_evaluate_scene(scene.get(),animation.get(),0,nullptr,&error),ufbx_free_scene);CHECK(sample);
    const auto expected=std::map<std::string,double>{{"Face",.605},{"Face2",.85},{"Face3",.08}};
    for(const auto *node:sample->nodes)if(node->mesh)
    {
        const std::string name(node->name.data,node->name.length);CHECK(expected.contains(name));
        CHECK(node->mesh->blend_deformers.count==1);
        const auto offset=ufbx_get_blend_vertex_offset(node->mesh->blend_deformers[0],0);
        CHECK(close(node->mesh->vertices[0].y+offset.y,expected.at(name)));
    }
    Object3DEngine roundEngine;const auto round=importObject3DFbxWithProgressiveMorphs(roundEngine.reference(),bytes.bytes);
    const auto restored=inspectObject3DProgressiveMorphRig(round.rig);
    CHECK(restored.channels.size()==2&&restored.channels[0].sourceMeshes.size()==2&&restored.channels[1].sourceMeshes.size()==2);
    auto actor=round.asset.object.get().instantiate();
    round.rig.get().applyTo(*actor,round.rig.get().evaluateNamed(std::array{NamedRigControl{"Expression",.55},NamedRigControl{"Blink",.2}}));
    for(auto *component:actor->components())if(auto *mesh=dynamic_cast<SkeletalMeshComponent *>(component))
        CHECK(close(mesh->deformedVertices()[0].position.y,expected.at(mesh->name())));

    const auto expr=object3DProgressiveControlCurveName("Expression"),blink=object3DProgressiveControlCurveName("Blink");
    AnimationClip clip(object.get().skeleton(),2,{},{{expr,{{0,0},{1,.55},{2,1}}},
        {blink,{{0,0},{1,.2},{2,.5}}},{"Other",{{0,0},{1,.5},{2,1}}}});
    const std::array bindings{AnimationMorphBinding{2,2,1}};
    FbxAnimationExportOptions options;options.indexedBindings=bindings;
    const auto animated=exportObject3DFbxWithProgressiveMorphs(rig,clip,options);
    Scene animatedScene(ufbx_load_memory(animated.bytes.data(),animated.bytes.size(),&strict,&error),ufbx_free_scene);
    CHECK(animatedScene&&animatedScene->anim_stacks.count==1);
    for(const auto *channel:animatedScene->blend_channels)
    {
        const std::string_view name(channel->name.data,channel->name.length);
        if(name=="Expression")CHECK(close(ufbx_evaluate_blend_weight(animatedScene->anim_stacks[0]->anim,channel,1),.55));
        if(name=="Blink")CHECK(close(ufbx_evaluate_blend_weight(animatedScene->anim_stacks[0]->anim,channel,1),.2));
    }
    Object3DEngine animatedEngine;const auto loaded=importObject3DFbxWithProgressiveMorphsAndAnimations(animatedEngine.reference(),animated.bytes);
    CHECK(loaded.animations.size()==1&&inspectObject3DProgressiveMorphRig(loaded.rig).channels.size()==2);
    auto moved=loaded.asset.object.get().instantiate();std::vector<SkeletalMeshComponent*> meshes;
    for(auto *component:moved->components())if(auto *mesh=dynamic_cast<SkeletalMeshComponent *>(component))meshes.push_back(mesh);
    applyObject3DProgressiveAnimationClip(loaded.rig,*loaded.animations[0].clip,loaded.animations[0].morphBindings,1,moved->pose(),meshes);
    for(auto *mesh:meshes)
        CHECK(close(mesh->deformedVertices()[0].position.y,expected.at(mesh->name())+(mesh->name()=="Face3"?.1:0)));
}
void invalid()
{
    Object3DEngine engine;const auto object=engine.createObject(data());const auto base=program();const auto count=engine.rigCount();
    const auto test=[&](const auto &candidate){rejects([&]{(void)createObject3DProgressiveMorphRig(object,candidate);});CHECK(engine.rigCount()==count);};
    auto value=base;value.channels.clear();test(value);
    value=base;value.channels[0].keys.clear();test(value);
    value=base;value.channels[0].keys[0].fullWeight=0;test(value);
    value=base;value.channels[0].keys[0].fullWeight=std::numeric_limits<double>::quiet_NaN();test(value);
    value=base;value.channels[0].keys[1].fullWeight=value.channels[0].keys[0].fullWeight;test(value);
    value=base;std::swap(value.channels[0].keys[0],value.channels[0].keys[1]);test(value);
    value=base;value.channels[0].keys[0].target="missing";test(value);
    value=base;value.channels[1].name=value.channels[0].name;test(value);
    value=base;value.channels[0].defaultValue=std::numeric_limits<double>::infinity();test(value);
    value=base;value.channels[0].sourceMeshes={"Different"};test(value);
    value=base;value.channels[0].sourceMeshes={"Face","Face"};test(value);
    value=base;value.channels[0].sourceMeshes.resize(65536,"Face");test(value);
    value=base;value.channels[0].keys.resize(65536);test(value);
    const auto rig=createObject3DProgressiveMorphRig(object,base);
    rejects([&]{(void)rig.get().evaluateNamed(std::array{NamedRigControl{"Expression",std::numeric_limits<double>::infinity()}});});
    rejects([&]{(void)rig.get().evaluateNamed(std::array{NamedRigControl{"unknown",.5}});});
    ProgressiveMorphProgram one{{{"One",0,{{"PositiveNear",.3}}}}};
    const auto oneRig=createObject3DProgressiveMorphRig(object,one);
    CHECK(close(output(oneRig.get(),oneRig.get().evaluateNamed(std::array{NamedRigControl{"One",-.15}}),"PositiveNear"),-.5));
    CHECK(close(output(oneRig.get(),oneRig.get().evaluateNamed(std::array{NamedRigControl{"One",.6}}),"PositiveNear"),2));
    one.channels[0].keys[0].fullWeight=-.2;const auto negative=createObject3DProgressiveMorphRig(object,one);
    CHECK(close(output(negative.get(),negative.get().evaluateNamed(std::array{NamedRigControl{"One",.1}}),"PositiveNear"),-.5));
    one.channels[0].keys[0].fullWeight=std::numeric_limits<double>::denorm_min();
    const auto tiny=createObject3DProgressiveMorphRig(object,one);
    rejects([&]{(void)tiny.get().evaluateNamed(std::array{NamedRigControl{"One",1}});});
    value=base;value.channels.push_back({"Add",0,{{"PositiveNear",1}}});
    const auto shared=createObject3DProgressiveMorphRig(object,value);
    const auto combined=shared.get().evaluateNamed(std::array{NamedRigControl{"Expression",.15},NamedRigControl{"Add",.5}});
    CHECK(close(output(shared.get(),combined,"PositiveNear"),1));
}
void project()
{
    Object3DEngine engine;const auto original=engine.createObject(data());const auto editor=engine.editObject(original);
    editor.get().setMorph(0,{"Blink",{{0,{0,.6,0},{}}}});const auto current=editor.get().object();
    const auto rig=createObject3DProgressiveMorphRig(current,program());
    const auto bytes=exportObject3DProject(editor,std::array{rig});CHECK(bytes.size()>24&&bytes[8]==31);
    Object3DEngine restoredEngine;const auto loaded=importObject3DProject(restoredEngine.reference(),bytes);
    CHECK(loaded.rigs.size()==1&&inspectObject3DProgressiveMorphRig(loaded.rigs[0])==program());
    const auto value=loaded.rigs[0].get().evaluateNamed(std::array{NamedRigControl{"Expression",.55}});
    CHECK(close(output(loaded.rigs[0].get(),value,"PositiveNear"),.5));
    CHECK(loaded.editor.get().undo()&&loaded.editor.get().redo());
    const auto roundtrip=exportObject3DProject(loaded);CHECK(roundtrip.size()>24&&roundtrip[8]==31);
}
void lifetime()
{
    Object3DRigHandle retained;
    {Object3DEngine engine;const auto object=engine.createObject(data());retained=createObject3DProgressiveMorphRig(object,program());CHECK(retained.valid());}
    CHECK(!retained.valid());rejects([&]{(void)inspectObject3DProgressiveMorphRig(retained);});
}
void archive()
{
    Object3DEngine engine;const auto object=engine.createObject(data());auto value=program();
    value.channels[0].sourceMeshes={"Face"};
    const auto rig=createObject3DProgressiveMorphRig(object,value);const auto editor=engine.editObject(object);
    const auto bytes=exportObject3DProject(editor,std::array{rig});
    for(const auto byte:bytes)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(byte);
    std::cout<<'\n';
}
void rejectedArchive()
{
    std::string encoded;std::cin>>encoded;CHECK(encoded.size()%2==0);
    std::vector<std::uint8_t> bytes;bytes.reserve(encoded.size()/2);
    const auto digit=[](char c)->unsigned{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::invalid_argument("Bad hex");};
    for(std::size_t i=0;i<encoded.size();i+=2)bytes.push_back(static_cast<std::uint8_t>(digit(encoded[i])*16+digit(encoded[i+1])));
    Object3DEngine engine;const auto sentinel=engine.createObject(data());
    rejects([&]{(void)importObject3DProject(engine.reference(),bytes);});
    CHECK(sentinel.valid()&&engine.objectCount()==1&&engine.rigCount()==0);
    const auto next=engine.createObject(data());CHECK(next.id()==sentinel.id()+1);
}
void reference(const char *path)
{
    ufbx_error error{};using Scene=std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)>;
    Scene scene(ufbx_load_file(path,nullptr,&error),ufbx_free_scene);CHECK(scene&&scene->blend_channels.count==1);
    const auto *channel=scene->blend_channels[0];CHECK(channel->keyframes.count==5);
    Object3DEngine engine;const auto object=engine.createObject(data());const auto rig=createObject3DProgressiveMorphRig(object,program());
    for(const double weight:{-2.,-.75,-.5,-.2,-.1,0.,.1,.3,.55,.8,1.,1.25,2.})
    {
        ufbx_prop_override_desc override{};override.element_id=channel->element_id;
        override.prop_name={"DeformPercent",13};override.value.x=weight*100;
        ufbx_anim_opts options{};options.prop_overrides={&override,1};
        std::unique_ptr<ufbx_anim,decltype(&ufbx_free_anim)> animation(ufbx_create_anim(scene.get(),&options,&error),ufbx_free_anim);
        CHECK(animation);Scene evaluated(ufbx_evaluate_scene(scene.get(),animation.get(),0,nullptr,&error),ufbx_free_scene);CHECK(evaluated);
        const auto *sample=evaluated->blend_channels[channel->typed_id];const auto result=rig.get().evaluateNamed(std::array{NamedRigControl{"Expression",weight}});
        for(const auto &key:sample->keyframes)
            CHECK(close(output(rig.get(),result,{key.shape->name.data,key.shape->name.length}),key.effective_weight));
        auto actor=object.get().instantiate();rig.get().applyTo(*actor,result);
        const auto expected=ufbx_get_blend_vertex_offset(evaluated->blend_deformers[0],0);
        CHECK(close(actor->findComponent<SkeletalMeshComponent>()->deformedVertices()[0].position.y,expected.y));
    }
    std::ifstream input(path,std::ios::binary);CHECK(input.good());const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};
    const auto count=engine.objectCount();bool unsupported=false;
    try{(void)importObject3DFbx(engine.reference(),bytes);}
    catch(const std::invalid_argument &error){unsupported=std::string_view(error.what()).find("Progressive FBX blend shapes")!=std::string_view::npos;}
    CHECK(unsupported&&engine.objectCount()==count);
}
void exported(const char *path,bool emit=false)
{
    std::ifstream input(path,std::ios::binary);CHECK(input.good());
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};
    Object3DEngine engine;const auto imported=importObject3DFbxWithProgressiveMorphs(engine.reference(),bytes);
    const auto sourceProgram=inspectObject3DProgressiveMorphRig(imported.rig);
    const auto editor=engine.editObject(imported.asset.object);
    const auto &targets=editor.get().object().get().data().meshes[0].topology->morphTargets;
    const auto found=std::find_if(targets.begin(),targets.end(),[&](const auto &target)
        {return target.name==sourceProgram.channels[0].keys[2].target;});
    CHECK(found!=targets.end());auto edited=*found;edited.deltas[0].position.y=.5;
    const auto mid=std::find_if(targets.begin(),targets.end(),[&](const auto &target)
        {return target.name==sourceProgram.channels[0].keys[3].target;});
    CHECK(mid!=targets.end()&&!mid->normalDeltas);auto editedMid=*mid;editedMid.deltas[0].position.z=.3;
    editor.get().setControlMorph(0,edited);
    editor.get().setControlMorph(0,editedMid);
    const auto updated=imported.rig.get().cloneFor(editor.get().object());
    const auto saved=exportObject3DProject(editor,std::array{updated});
    Object3DEngine restoredEngine;const auto restored=importObject3DProject(restoredEngine.reference(),saved);
    const auto result=exportObject3DFbxWithProgressiveMorphs(restored.rigs[0]);
    CHECK(!result.bytes.empty()&&!result.warnings.empty());
    if(emit)
    {
        for(const auto byte:result.bytes)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(byte);
        std::cout<<'\n';return;
    }
    ufbx_error error{};using Scene=std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)>;
    ufbx_load_opts strict{};strict.strict=true;strict.file_format=UFBX_FILE_FORMAT_FBX;
    Scene scene(ufbx_load_memory(result.bytes.data(),result.bytes.size(),&strict,&error),ufbx_free_scene);
    if(!scene){char message[4096]{};ufbx_format_error(message,sizeof(message),&error);std::cerr<<message<<'\n';}
    CHECK(scene&&scene->blend_channels.count==1&&scene->blend_channels[0]->keyframes.count==5);
    const auto *channel=scene->blend_channels[0];CHECK(close(channel->weight,.15));
    Object3DEngine secondEngine;const auto roundtrip=importObject3DFbxWithProgressiveMorphs(secondEngine.reference(),result.bytes);
    CHECK(roundtrip.rig.valid());const auto roundProgram=inspectObject3DProgressiveMorphRig(roundtrip.rig);
    CHECK(roundProgram.channels.size()==1&&roundProgram.channels[0].keys.size()==5);
    FbxImportOptions withoutMetadata;withoutMetadata.restoreAuthoringMetadata=false;
    Object3DEngine unextended;CHECK(importObject3DFbxWithProgressiveMorphs(unextended.reference(),result.bytes,withoutMetadata).rig.valid());
    auto malformed=result.bytes;const std::string_view marker="iiActorObjectProgressive";
    const auto foundMarker=std::search(malformed.begin(),malformed.end(),marker.begin(),marker.end());CHECK(foundMarker!=malformed.end());
    const auto markerOffset=static_cast<std::size_t>(foundMarker-malformed.begin())+marker.size();
    CHECK(markerOffset+5<=malformed.size()&&malformed[markerOffset]=='I');malformed[markerOffset+1]=2;
    const auto objectCount=secondEngine.objectCount(),rigCount=secondEngine.rigCount();
    rejects([&]{(void)importObject3DFbxWithProgressiveMorphs(secondEngine.reference(),malformed);});
    CHECK(secondEngine.objectCount()==objectCount&&secondEngine.rigCount()==rigCount);
    const auto &roundTargets=roundtrip.asset.object.get().data().meshes[0].topology->morphTargets;
    const auto roundNear=std::find_if(roundTargets.begin(),roundTargets.end(),[&](const auto &item)
        {return item.name==roundProgram.channels[0].keys[2].target;});
    CHECK(roundNear!=roundTargets.end()&&roundNear->normalDeltas&&!roundNear->normalDeltas->empty());
    CHECK(close(std::abs(roundNear->normalDeltas->front().normal.x),.1));
    auto actor=roundtrip.asset.object.get().instantiate();auto *mesh=actor->findComponent<SkeletalMeshComponent>();CHECK(mesh);
    auto expectedActor=restored.rigs[0].get().object().get().instantiate();
    restored.rigs[0].get().applyTo(*expectedActor,restored.rigs[0].get().evaluateNamed(std::array{NamedRigControl{"Expression",.8}}));
    roundtrip.rig.get().applyTo(*actor,roundtrip.rig.get().evaluateNamed(std::array{NamedRigControl{roundProgram.channels[0].name,.8}}));
    const auto expectedNormal=expectedActor->findComponent<SkeletalMeshComponent>()->deformedVertices()[0].normal;
    const auto actualNormal=mesh->deformedVertices()[0].normal;
    CHECK(close(expectedNormal.x,actualNormal.x)&&close(expectedNormal.y,actualNormal.y)&&close(expectedNormal.z,actualNormal.z));
    for(double weight:{-1.25,-.75,-.4,-.2,0.,.15,.3,.55,.8,1.,1.3})
    {
        ufbx_prop_override_desc override{};override.element_id=channel->element_id;
        override.prop_name={"DeformPercent",13};override.value.x=weight*100;
        ufbx_anim_opts options{};options.prop_overrides={&override,1};
        std::unique_ptr<ufbx_anim,decltype(&ufbx_free_anim)> animation(ufbx_create_anim(scene.get(),&options,&error),ufbx_free_anim);
        CHECK(animation);Scene sample(ufbx_evaluate_scene(scene.get(),animation.get(),0,nullptr,&error),ufbx_free_scene);CHECK(sample);
        roundtrip.rig.get().applyTo(*actor,roundtrip.rig.get().evaluateNamed(std::array{NamedRigControl{roundProgram.channels[0].name,weight}}));
        const auto expected=ufbx_get_blend_vertex_offset(sample->blend_deformers[0],0);
        CHECK(close(mesh->deformedVertices()[0].position.y,expected.y));
    }
    const auto editedDefaults=engine.editObject(imported.asset.object);
    editedDefaults.get().setDefaultMorphWeight(0,sourceProgram.channels[0].keys[2].target,.6);
    const auto conflicting=imported.rig.get().cloneFor(editedDefaults.get().object());
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(conflicting);});
    FbxExportOptions external;external.externalDocument="document";
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(updated,external);});
    auto instanced=data();auto copy=instanced.meshes[0];copy.name="Face2";instanced.meshes.push_back(copy);
    auto instanceProgram=program();instanceProgram.channels[0].defaultValue=0;
    const auto instancedObject=engine.createObject(instanced);const auto instancedRig=createObject3DProgressiveMorphRig(instancedObject,instanceProgram);
    const auto instanceBytes=exportObject3DFbxWithProgressiveMorphs(instancedRig);
    Scene instanceScene(ufbx_load_memory(instanceBytes.bytes.data(),instanceBytes.bytes.size(),&strict,&error),ufbx_free_scene);
    CHECK(instanceScene&&instanceScene->meshes.count==1&&instanceScene->meshes[0]->instances.count==2&&
          instanceScene->skin_deformers.count==1);
    Object3DEngine instanceEngine;const auto instanceRound=importObject3DFbxWithProgressiveMorphs(instanceEngine.reference(),instanceBytes.bytes);
    const auto instanceRoundProgram=inspectObject3DProgressiveMorphRig(instanceRound.rig);
    CHECK(instanceRoundProgram.channels.size()==2&&instanceRoundProgram.channels[0].sourceMeshes.size()==2);
    auto instanceActor=instanceRound.asset.object.get().instantiate();
    instanceRound.rig.get().applyTo(*instanceActor,instanceRound.rig.get().evaluateNamed(std::array{NamedRigControl{"Expression",.3}}));
    std::vector<double> instancePositions;
    for(auto *component:instanceActor->components())if(auto *part=dynamic_cast<SkeletalMeshComponent *>(component))
        instancePositions.push_back(part->deformedVertices()[0].position.y);
    CHECK(instancePositions.size()==2&&close(instancePositions[0],.25)&&close(instancePositions[1],.25));
    auto diverged=instanced;diverged.meshes[1].geometry.morphTargets[2].deltas[0].position.y=.9;
    const auto divergedRig=createObject3DProgressiveMorphRig(engine.createObject(diverged),instanceProgram);
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(divergedRig);});
    auto differentWeights=instanced;differentWeights.meshes[1].geometry.vertices[0].weights={{1,1}};
    const auto differentWeightsRig=createObject3DProgressiveMorphRig(engine.createObject(differentWeights),instanceProgram);
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(differentWeightsRig);});
    auto independentMorph=instanced;
    for(auto &mesh:independentMorph.meshes)mesh.geometry.morphTargets.push_back({"Other",{{0,{0,.1,0},{}}}});
    const auto independentRig=createObject3DProgressiveMorphRig(engine.createObject(independentMorph),instanceProgram);
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(independentRig);});
    auto overlapping=instanced;overlapping.meshes[1].geometry.morphTargets.pop_back();
    const auto overlappingRig=createObject3DProgressiveMorphRig(engine.createObject(overlapping),instanceProgram);
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(overlappingRig);});
    auto singleData=data();ProgressiveMorphProgram single{{{"Only",0,{{"Blink",1}}}}};
    const auto singleObject=engine.createObject(singleData);const auto singleRig=createObject3DProgressiveMorphRig(singleObject,single);
    const auto singleBytes=exportObject3DFbxWithProgressiveMorphs(singleRig);
    Object3DEngine singleEngine;const auto singleRound=importObject3DFbxWithProgressiveMorphs(singleEngine.reference(),singleBytes.bytes);
    CHECK(singleRound.rig.valid()&&inspectObject3DProgressiveMorphRig(singleRound.rig).channels.size()==1);
    auto sharedData=data();sharedData.meshes[0].geometry.skinning.method=SkinningMethod::DualQuaternion;
    sharedData.meshes[0].morphDefaults={{"NegativeFar",.2}};
    ProgressiveMorphProgram shared{{{"A",0,{{"Blink",1}}},{"B",0,{{"Blink",.5}}}}};
    const auto sharedObject=engine.createObject(sharedData);const auto sharedRig=createObject3DProgressiveMorphRig(sharedObject,shared);
    const auto sharedBytes=exportObject3DFbxWithProgressiveMorphs(sharedRig);
    Scene sharedScene(ufbx_load_memory(sharedBytes.bytes.data(),sharedBytes.bytes.size(),&strict,&error),ufbx_free_scene);
    CHECK(sharedScene&&sharedScene->skin_deformers.count==1&&
          sharedScene->skin_deformers[0]->skinning_method==UFBX_SKINNING_METHOD_DUAL_QUATERNION);
    Object3DEngine sharedEngine;const auto sharedRound=importObject3DFbxWithProgressiveMorphs(sharedEngine.reference(),sharedBytes.bytes);
    const auto sharedProgram=inspectObject3DProgressiveMorphRig(sharedRound.rig);CHECK(sharedProgram.channels.size()==2);
    auto sharedActor=sharedRound.asset.object.get().instantiate();
    sharedRound.rig.get().applyTo(*sharedActor,sharedRound.rig.get().evaluateNamed(
        std::array{NamedRigControl{"A",.4},NamedRigControl{"B",.25}}));
    CHECK(close(sharedActor->findComponent<SkeletalMeshComponent>()->deformedVertices()[0].position.y,.24));
    Object3DData seamData;seamData.bones={{"root",NoBone,{}}};Object3DMesh seamMesh;seamMesh.name="Seam";MeshTopology topology;
    for(auto point:std::array{Vector3{0,0,0},Vector3{1,0,0},Vector3{2,0,0},Vector3{0,1,0},Vector3{1,1,0},Vector3{2,1,0}})
        topology.points.push_back({point,{{0,1}}});
    topology.normals={{0,0,1},{0,0,1}};topology.textureCoordinates={{0,0},{1,0},{1,1},{0,1},{0,0},{1,0},{1,1},{0,1}};
    MeshTopologyDefinition layout;layout.sourceFormat="fixture";layout.positionCount=6;layout.textureCoordinateCount=8;layout.normalCount=2;
    layout.layouts={{0,0,0},{1,1,0},{4,2,0},{3,3,0},{1,4,1},{2,5,1},{5,6,1},{4,7,1}};
    layout.polygons={{0,1,2,3},{4,5,6,7}};topology.layout=MeshTopologyLayout::create(std::move(layout),topology.points);
    TopologyMorph seamMorph;seamMorph.name="SeamShape";
    seamMorph.normalDeltas=std::vector<TopologyNormalDelta>{{1,{.2,0,0}},{4,{0,.2,0}}};
    topology.morphTargets.push_back(seamMorph);seamMesh.topology=std::move(topology);seamData.meshes.push_back(std::move(seamMesh));
    const auto seamObject=engine.createObject(seamData);
    const auto seamRig=createObject3DProgressiveMorphRig(seamObject,{{{"SeamControl",0,{{"SeamShape",1}}}}});
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(seamRig);});
    FbxExportOptions permissive;permissive.allowMorphNormalMetadata=true;
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(seamRig,permissive);});
    auto duplicateData=imported.asset.object.get().data();
    duplicateData.meshes[0].topology->morphTargets.push_back(duplicateData.meshes[0].topology->morphTargets[2]);
    const auto duplicateObject=engine.createObject(duplicateData);
    const auto duplicateRig=imported.rig.get().cloneFor(duplicateObject);
    rejects([&]{(void)exportObject3DFbxWithProgressiveMorphs(duplicateRig);});
#if IIACTOROBJECT_HAS_NSCHARACTER
    Object3DEngine characterEngine;const auto character=NSCharacter::importFbxWithProgressiveMorphs(characterEngine,bytes);
    const auto humanBytes=NSCharacter::exportFbxWithProgressiveMorphs(*character.asset,character.rig);
    CHECK(!humanBytes.bytes.empty());
    rejects([&]{(void)NSCharacter::exportFbxWithProgressiveMorphs(*character.asset,updated);});
#endif
}
void imported(const char *path)
{
    std::ifstream input(path,std::ios::binary);CHECK(input.good());
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};
    ufbx_error parseError{};ufbx_load_opts strict{};strict.file_format=UFBX_FILE_FORMAT_FBX;strict.strict=true;
    std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)> strictScene(ufbx_load_memory(bytes.data(),bytes.size(),&strict,&parseError),ufbx_free_scene);
    if(!strictScene){char message[4096]{};ufbx_format_error(message,sizeof(message),&parseError);std::cerr<<message<<'\n';}
    CHECK(strictScene&&strictScene->metadata.version==7400);
    Object3DEngine engine;const auto result=importObject3DFbxWithProgressiveMorphs(engine.reference(),bytes);
    CHECK(result.asset.object.valid()&&result.rig.valid());
    const auto source=inspectObject3DProgressiveMorphRig(result.rig);
    CHECK(source.channels.size()==1&&source.channels[0].name=="Expression");
    CHECK(source.channels[0].sourceName=="Expression"&&source.channels[0].sourceMeshes==std::vector<std::string>{"Face"});
    CHECK(source.channels[0].keys.size()==5&&close(source.channels[0].defaultValue,.15));
    CHECK(source.channels[0].keys[0].sourceName=="NegativeFar");
    CHECK(!result.asset.warnings.empty());
    auto actor=result.asset.object.get().instantiate();auto *mesh=actor->findComponent<SkeletalMeshComponent>();CHECK(mesh);
    CHECK(close(mesh->deformedVertices()[0].position.y,.125));
    const auto evaluated=result.rig.get().evaluateNamed(std::array{NamedRigControl{"Expression",.55}});
    result.rig.get().applyTo(*actor,evaluated);CHECK(close(mesh->deformedVertices()[0].position.y,.525));
    const auto editor=engine.editObject(result.asset.object);
    const auto &targets=editor.get().object().get().data().meshes[0].topology->morphTargets;
    const auto near=std::find_if(targets.begin(),targets.end(),[&](const auto &item){return item.name==source.channels[0].keys[2].target;});
    CHECK(near!=targets.end()&&!near->deltas.empty()&&near->normalDeltas&&!near->normalDeltas->empty());
    CHECK(close(std::abs(near->normalDeltas->front().normal.x),.1)&&
          close(near->normalDeltas->front().normal.y,0)&&close(near->normalDeltas->front().normal.z,0));
    const auto savedNormals=near->normalDeltas;auto edited=*near;edited.deltas[0].position.y=.5;
    editor.get().setControlMorph(0,std::move(edited));const auto editedRig=result.rig.get().cloneFor(editor.get().object());
    auto preview=editor.get().object().get().instantiate();editedRig.get().applyTo(*preview,editedRig.get().evaluateNamed(std::array{NamedRigControl{"Expression",.55}}));
    CHECK(close(preview->findComponent<SkeletalMeshComponent>()->deformedVertices()[0].position.y,.65));
    const auto bytesProject=exportObject3DProject(editor,std::array{editedRig});
    Object3DEngine destination;const auto loaded=importObject3DProject(destination.reference(),bytesProject);
    CHECK(loaded.rigs.size()==1&&inspectObject3DProgressiveMorphRig(loaded.rigs[0])==source);
    const auto &loadedTargets=loaded.editor.get().object().get().data().meshes[0].topology->morphTargets;
    const auto loadedNear=std::find_if(loadedTargets.begin(),loadedTargets.end(),[&](const auto &item){return item.name==source.channels[0].keys[2].target;});
    CHECK(loadedNear!=loadedTargets.end()&&loadedNear->normalDeltas==savedNormals);
    auto restored=loaded.rigs[0].get().object().get().instantiate();
    loaded.rigs[0].get().applyTo(*restored,loaded.rigs[0].get().evaluateNamed(std::array{NamedRigControl{"Expression",.55}}));
    CHECK(close(restored->findComponent<SkeletalMeshComponent>()->deformedVertices()[0].position.y,.65));
    CHECK(loaded.editor.get().undo()&&loaded.editor.get().redo());
#if IIACTOROBJECT_HAS_NSCHARACTER
    Object3DEngine characterEngine;const auto character=NSCharacter::importFbxWithProgressiveMorphs(characterEngine,bytes);
    CHECK(character.asset&&character.rig.valid()&&character.rig.get().object()==character.asset->object());
    CHECK(inspectObject3DProgressiveMorphRig(character.rig)==source);
#endif
    ufbx_error error{};using Scene=std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)>;
    Scene original(ufbx_load_file(path,nullptr,&error),ufbx_free_scene);CHECK(original&&original->blend_channels.count==1);
    const auto *originalChannel=original->blend_channels[0];
    for(double weight:{-1.25,-.75,-.4,-.2,0.,.15,.3,.55,.8,1.,1.3})
    {
        ufbx_prop_override_desc override{};override.element_id=originalChannel->element_id;
        override.prop_name={"DeformPercent",13};override.value.x=weight*100;
        ufbx_anim_opts options{};options.prop_overrides={&override,1};
        std::unique_ptr<ufbx_anim,decltype(&ufbx_free_anim)> animation(ufbx_create_anim(original.get(),&options,&error),ufbx_free_anim);
        CHECK(animation);Scene sample(ufbx_evaluate_scene(original.get(),animation.get(),0,nullptr,&error),ufbx_free_scene);CHECK(sample);
        const auto value=result.rig.get().evaluateNamed(std::array{NamedRigControl{"Expression",weight}});
        for(const auto &key:sample->blend_channels[originalChannel->typed_id]->keyframes)
        {
            const auto found=std::find_if(source.channels[0].keys.begin(),source.channels[0].keys.end(),[&](const auto &item)
                {return item.sourceName==std::string_view(key.shape->name.data,key.shape->name.length);});
            CHECK(found!=source.channels[0].keys.end()&&close(output(result.rig.get(),value,found->target),key.effective_weight));
        }
        result.rig.get().applyTo(*actor,value);const auto expected=ufbx_get_blend_vertex_offset(sample->blend_deformers[0],0);
        CHECK(close(mesh->deformedVertices()[0].position.y,expected.y));
    }
    const std::string originalText(bytes.begin(),bytes.end());
    const auto changed=[&](std::string_view before,std::string_view after)
    {
        auto text=originalText;const auto position=text.find(before);CHECK(position!=text.npos);
        text.replace(position,before.size(),after);return std::vector<std::uint8_t>(text.begin(),text.end());
    };
    const auto objects=engine.objectCount(),rigs=engine.rigCount();
    for(const auto &bad:{changed("-75,-20,30,80,100","-75,-20,0,80,100"),
                         changed("-75,-20,30,80,100","-75,-20,30,30,100"),
                         changed("Geometry::NegativeFar",std::string("Geometry::")+std::string(1025,'x'))})
    {
        rejects([&]{(void)importObject3DFbxWithProgressiveMorphs(engine.reference(),bad);});
        CHECK(engine.objectCount()==objects&&engine.rigCount()==rigs);
    }
    FbxImportOptions bounded;bounded.maxMorphsPerMesh=4;
    rejects([&]{(void)importObject3DFbxWithProgressiveMorphs(engine.reference(),bytes,bounded);});
    CHECK(engine.objectCount()==objects&&engine.rigCount()==rigs);
    const auto metadata=changed("DeformPercent: 15\n  FullWeights", "DeformPercent: 15\n  iiNormalMode: 0\n  FullWeights");
    rejects([&]{(void)importObject3DFbxWithProgressiveMorphs(engine.reference(),metadata);});
    FbxImportOptions ignoreMetadata;ignoreMetadata.restoreAuthoringMetadata=false;
    CHECK(importObject3DFbxWithProgressiveMorphs(engine.reference(),metadata,ignoreMetadata).rig.valid());
    auto multiText=originalText;
    const auto model=multiText.find(" Model: 101, \"Model::Face\", \"Mesh\" { Version: 232 }");CHECK(model!=multiText.npos);
    multiText.insert(model," Model: 102, \"Model::Face2\", \"Mesh\" { Version: 232 }\n");
    const auto connection=multiText.find(" C: \"OO\",100,101");CHECK(connection!=multiText.npos);
    multiText.insert(connection," C: \"OO\",100,102\n C: \"OO\",102,0\n");
    Object3DEngine multiEngine;const std::vector<std::uint8_t> multiBytes(multiText.begin(),multiText.end());
    const auto multi=importObject3DFbxWithProgressiveMorphs(multiEngine.reference(),multiBytes);
    const auto multiProgram=inspectObject3DProgressiveMorphRig(multi.rig);CHECK(multiProgram.channels.size()==1);
    CHECK(multiProgram.channels[0].name=="Expression"&&multiProgram.channels[0].sourceMeshes.size()==2);
    const auto multiExport=exportObject3DFbxWithProgressiveMorphs(multi.rig);
    Object3DEngine reimportMultiEngine;const auto reimportMulti=importObject3DFbxWithProgressiveMorphs(reimportMultiEngine.reference(),multiExport.bytes);
    CHECK(inspectObject3DProgressiveMorphRig(reimportMulti.rig).channels[0].sourceMeshes.size()==2);
    auto multiActor=multi.asset.object.get().instantiate();const auto multiValue=multi.rig.get().evaluateNamed(std::array{NamedRigControl{"Expression",.3}});
    multi.rig.get().applyTo(*multiActor,multiValue);std::vector<double> positions;
    for(auto *component:multiActor->components())if(auto *part=dynamic_cast<SkeletalMeshComponent *>(component))positions.push_back(part->deformedVertices()[0].position.y);
    CHECK(positions.size()==2&&close(positions[0],.25)&&close(positions[1],.25));
    auto separate=originalText;const auto objectsEnd=separate.find("}\nConnections: {");CHECK(objectsEnd!=separate.npos);
    separate.insert(objectsEnd,
        " Deformer: 202, \"SubDeformer::Expression\", \"BlendShapeChannel\" {\n"
        "  Version: 100\n  DeformPercent: 0\n  FullWeights: *2 { a: 50,100 }\n }\n"
        " Geometry: 215, \"Geometry::OtherHalf\", \"Shape\" {\n  Version: 100\n  Indexes: *1 { a: 0 }\n  Vertices: *3 { a: 0,0.2,0 }\n }\n"
        " Geometry: 216, \"Geometry::OtherFull\", \"Shape\" {\n  Version: 100\n  Indexes: *1 { a: 0 }\n  Vertices: *3 { a: 0,0.4,0 }\n }\n");
    const auto firstConnection=separate.find(" C: \"OO\",201,200");CHECK(firstConnection!=separate.npos);
    separate.insert(firstConnection," C: \"OO\",202,200\n C: \"OO\",215,202\n C: \"OO\",216,202\n");
    Object3DEngine separateEngine;const std::vector<std::uint8_t> separateBytes(separate.begin(),separate.end());
    const auto separateResult=importObject3DFbxWithProgressiveMorphs(separateEngine.reference(),separateBytes);
    const auto separateProgram=inspectObject3DProgressiveMorphRig(separateResult.rig);
    CHECK(separateProgram.channels.size()==2&&separateProgram.channels[0].name!=separateProgram.channels[1].name);
    CHECK(separateProgram.channels[0].sourceName=="Expression"&&separateProgram.channels[1].sourceName=="Expression");
    std::vector<NamedRigControl> controls;
    for(const auto &channel:separateProgram.channels)controls.push_back({channel.name,channel.keys.size()==5?.3:.5});
    auto separateActor=separateResult.asset.object.get().instantiate();
    separateResult.rig.get().applyTo(*separateActor,separateResult.rig.get().evaluateNamed(controls));
    CHECK(close(separateActor->findComponent<SkeletalMeshComponent>()->deformedVertices()[0].position.y,.45));
    auto single=originalText;
    const auto full=single.find("FullWeights: *5 { a: -75,-20,30,80,100 }");CHECK(full!=single.npos);
    single.replace(full,std::string_view("FullWeights: *5 { a: -75,-20,30,80,100 }").size(),"FullWeights: *1 { a: 99.99995 }");
    for(const auto *connection:{" C: \"OO\",211,201\n"," C: \"OO\",212,201\n"," C: \"OO\",213,201\n"," C: \"OO\",214,201\n"})
    {const auto at=single.find(connection);CHECK(at!=single.npos);single.erase(at,std::string_view(connection).size());}
    const std::vector<std::uint8_t> singleBytes(single.begin(),single.end());Object3DEngine singleEngine;
    const auto singleResult=importObject3DFbxWithProgressiveMorphs(singleEngine.reference(),singleBytes);
    const auto singleProgram=inspectObject3DProgressiveMorphRig(singleResult.rig);
    CHECK(singleProgram.channels.size()==1&&singleProgram.channels[0].keys.size()==1&&close(singleProgram.channels[0].keys[0].fullWeight,.9999995));
    const auto disconnected=changed(" C: \"OO\",201,200\n","");Object3DEngine disconnectedEngine;
    const auto noProgressive=importObject3DFbxWithProgressiveMorphs(disconnectedEngine.reference(),disconnected);
    CHECK(noProgressive.asset.object.valid()&&!noProgressive.rig.valid());
    auto animated=originalText;const auto animationObjectsEnd=animated.find("}\nConnections: {");CHECK(animationObjectsEnd!=animated.npos);
    animated.insert(animationObjectsEnd,
        " AnimationStack: 300, \"AnimStack::Take\", \"\" {\n"
        "  Properties70: { P: \"LocalStart\", \"KTime\", \"Time\", \"\",0\n"
        "   P: \"LocalStop\", \"KTime\", \"Time\", \"\",46186158000 }\n }\n"
        " AnimationLayer: 301, \"AnimLayer::Base\", \"\" { }\n");
    const auto animationConnection=animated.find(" C: \"OO\",201,200");CHECK(animationConnection!=animated.npos);
    animated.insert(animationConnection," C: \"OO\",301,300\n");
    const std::vector<std::uint8_t> animationBytes(animated.begin(),animated.end());Object3DEngine animationEngine;
    const auto animationResult=importObject3DFbxWithProgressiveMorphs(animationEngine.reference(),animationBytes);
    CHECK(std::any_of(animationResult.asset.warnings.begin(),animationResult.asset.warnings.end(),[](const auto &warning)
        {return warning.find("progressive animation import is not yet supported")!=warning.npos;}));
    auto skinned=originalText;const auto skinObjects=skinned.find("}\nConnections: {");CHECK(skinObjects!=skinned.npos);
    skinned.insert(skinObjects,
        " Model: 500, \"Model::Bone\", \"LimbNode\" { Version: 232\n"
        "  Properties70: { P: \"Lcl Translation\", \"Lcl Translation\", \"\", \"A\",0,0,0\n"
        "   P: \"Lcl Rotation\", \"Lcl Rotation\", \"\", \"A\",0,0,0\n"
        "   P: \"Lcl Scaling\", \"Lcl Scaling\", \"\", \"A\",1,1,1 }\n }\n"
        " NodeAttribute: 501, \"NodeAttribute::Bone\", \"LimbNode\" { TypeFlags: \"Skeleton\" }\n"
        " Deformer: 400, \"Deformer::Skin\", \"Skin\" { Version: 101 }\n"
        " Deformer: 401, \"SubDeformer::Cluster\", \"Cluster\" { Version: 100\n"
        "  Indexes: *3 { a: 0,1,2 }\n  Weights: *3 { a: 1,1,1 }\n"
        "  Transform: *16 { a: 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 }\n"
        "  TransformLink: *16 { a: 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 }\n }\n");
    const auto skinConnection=skinned.find(" C: \"OO\",201,200");CHECK(skinConnection!=skinned.npos);
    skinned.insert(skinConnection," C: \"OO\",400,100\n C: \"OO\",401,400\n C: \"OO\",500,401\n C: \"OO\",501,500\n C: \"OO\",500,0\n");
    Object3DEngine skinEngine;const std::vector<std::uint8_t> skinBytes(skinned.begin(),skinned.end());
    const auto skin=importObject3DFbxWithProgressiveMorphs(skinEngine.reference(),skinBytes);
    CHECK(skin.asset.object.get().hasRig());const auto bone=skin.asset.object.get().skeleton()->findBone("Bone");CHECK(bone!=NoBone);
    auto skinnedActor=skin.asset.object.get().instantiate();auto local=skinnedActor->pose().locals()[bone];local.position.y+=1;
    skinnedActor->pose().setLocal(bone,local);skin.rig.get().applyTo(*skinnedActor,skin.rig.get().evaluateNamed(std::array{NamedRigControl{"Expression",.55}}));
    CHECK(skinnedActor->pose().locals()[bone]==local);
    CHECK(close(skinnedActor->findComponent<SkeletalMeshComponent>()->deformedVertices()[0].position.y,1.525));
    auto sharedSkin=skinned;const auto secondModel=sharedSkin.find(" Model: 101, \"Model::Face\", \"Mesh\" { Version: 232 }");
    CHECK(secondModel!=sharedSkin.npos);sharedSkin.insert(secondModel," Model: 102, \"Model::Face2\", \"Mesh\" { Version: 232 }\n");
    const auto secondConnection=sharedSkin.find(" C: \"OO\",100,101");CHECK(secondConnection!=sharedSkin.npos);
    sharedSkin.insert(secondConnection," C: \"OO\",100,102\n C: \"OO\",102,0\n");
    Object3DEngine sharedSkinEngine;const std::vector<std::uint8_t> sharedSkinBytes(sharedSkin.begin(),sharedSkin.end());
    const auto sharedSkinImport=importObject3DFbxWithProgressiveMorphs(sharedSkinEngine.reference(),sharedSkinBytes);
    const auto sharedSkinFbx=exportObject3DFbxWithProgressiveMorphs(sharedSkinImport.rig);
    ufbx_error sharedError{};Scene sharedSkinScene(ufbx_load_memory(sharedSkinFbx.bytes.data(),sharedSkinFbx.bytes.size(),nullptr,&sharedError),ufbx_free_scene);
    CHECK(sharedSkinScene&&sharedSkinScene->meshes.count==1&&sharedSkinScene->meshes[0]->instances.count==2&&sharedSkinScene->skin_deformers.count==1);
    Object3DEngine sharedSkinRoundEngine;const auto sharedSkinRound=importObject3DFbxWithProgressiveMorphs(sharedSkinRoundEngine.reference(),sharedSkinFbx.bytes);
    auto sharedSkinActor=sharedSkinRound.asset.object.get().instantiate();
    const auto roundBone=sharedSkinRound.asset.object.get().skeleton()->findBone("Bone");CHECK(roundBone!=NoBone);
    auto roundLocal=sharedSkinActor->pose().locals()[roundBone];roundLocal.position.y+=1;sharedSkinActor->pose().setLocal(roundBone,roundLocal);
    sharedSkinRound.rig.get().applyTo(*sharedSkinActor,sharedSkinRound.rig.get().evaluateNamed(std::array{NamedRigControl{"Expression",.55}}));
    std::size_t sharedParts=0;
    for(auto *component:sharedSkinActor->components())if(auto *part=dynamic_cast<SkeletalMeshComponent *>(component))
    {++sharedParts;CHECK(close(part->deformedVertices()[0].position.y,1.525));}
    CHECK(sharedParts==2);
}
void emitShared(const char *path)
{
    std::ifstream input(path,std::ios::binary);CHECK(input.good());
    std::string source{std::istreambuf_iterator<char>(input),{}};
    const auto model=source.find(" Model: 101, \"Model::Face\", \"Mesh\" { Version: 232 }");CHECK(model!=source.npos);
    source.insert(model," Model: 102, \"Model::Face2\", \"Mesh\" { Version: 232 }\n");
    const auto connection=source.find(" C: \"OO\",100,101");CHECK(connection!=source.npos);
    source.insert(connection," C: \"OO\",100,102\n C: \"OO\",102,0\n");
    const std::vector<std::uint8_t> bytes(source.begin(),source.end());
    Object3DEngine engine;const auto imported=importObject3DFbxWithProgressiveMorphs(engine.reference(),bytes);
    const auto exported=exportObject3DFbxWithProgressiveMorphs(imported.rig);
    for(const auto byte:exported.bytes)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(byte);
    std::cout<<'\n';
}
int main(int argc,char **argv)
{
    try{if(argc<2)return 2;const std::string_view mode=argv[1];
        if(mode=="curve")curve();else if(mode=="invalid")invalid();else if(mode=="project")project();else if(mode=="split")split();
        else if(mode=="lifetime")lifetime();else if(mode=="archive"){archive();return 0;}else if(mode=="reject_archive")rejectedArchive();
        else if(mode=="reference"&&argc==3)reference(argv[2]);
        else if(mode=="import"&&argc==3)imported(argv[2]);
        else if(mode=="export"&&argc==3)exported(argv[2]);
        else if(mode=="emit_export"&&argc==3){exported(argv[2],true);return 0;}
        else if(mode=="emit_shared"&&argc==3){emitShared(argv[2]);return 0;}else return 2;
        std::cout<<mode<<" passed\n";return 0;}
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
