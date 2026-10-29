#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#include <iiActorObject/NSCharacter/HumanMorphology.h>
#include <iiActorObject/NSCharacter/HumanSkin.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DEditorRig.h>
#include <iiActorObject/Object3DInstanceTransfer.h>
#include <iiActorObject/Object3DProgressiveMorph.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
#define CHECK(x) do { if(!(x))throw std::runtime_error(#x); } while(false)
template<class F> void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}CHECK(failed);}
double distance(Vector3 a,Vector3 b){return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);}
void sameTransforms(std::span<const Transform> a,std::span<const Transform> b)
{
    CHECK(a.size()==b.size());
    for(std::size_t i=0;i<a.size();++i)
    {
        const auto &x=a[i],&y=b[i];
        const auto angle=1-std::abs(x.rotation.x*y.rotation.x+x.rotation.y*y.rotation.y+x.rotation.z*y.rotation.z+x.rotation.w*y.rotation.w);
        const auto position=distance(x.position,y.position),scale=distance(x.scale,y.scale);
        if(position>1e-9||scale>1e-9||angle>1e-9)
            throw std::runtime_error("Rig roundtrip joint "+std::to_string(i)+" position="+std::to_string(position)+" scale="+std::to_string(scale)+" rotation="+std::to_string(angle));
    }
}
std::size_t findMesh(const Object3DData &d,std::string_view name)
{
    const auto it=std::find_if(d.meshes.begin(),d.meshes.end(),[&](const auto &mesh){return mesh.name==name;});
    CHECK(it!=d.meshes.end());return static_cast<std::size_t>(it-d.meshes.begin());
}
HumanAccessorySelection selection(const std::filesystem::path &dir)
{
    return {dir,{"hair/bob01","clothes/female_casualsuit01","eyebrows/eyebrow001","eyelashes/eyelashes01"},true};
}
void catalog(const std::filesystem::path &dir)
{
    const auto entries=listMakeHumanAccessories(dir);CHECK(entries.size()==6);
    CHECK(std::is_sorted(entries.begin(),entries.end(),[](const auto &a,const auto &b){return a.id<b.id;}));
    CHECK(std::any_of(entries.begin(),entries.end(),[](const auto &a){return a.id=="hair/bob01"&&a.category=="hair";}));
    rejects([&]{(void)listMakeHumanAccessories(dir,10);});
    rejects([&]{(void)listMakeHumanAccessories(dir.parent_path()/"fixtures/MakeHumanWardrobeInvalid");});
}
void human(const std::filesystem::path &base,const std::filesystem::path &dir)
{
    Object3DEngine engine;MakeHumanTemplateOptions options;options.accessories=selection(dir);
    const auto human=loadMakeHumanTemplate(engine.reference(),base,options);const auto neutral=human.generate();
    const auto &data=neutral->data();CHECK(data.bones.size()==163&&data.meshes.size()==5&&data.images.size()==5);
    CHECK(human.program().get().definition().surfaces.size()==4);
    const auto hair=findMesh(data,"HairBob01"),suit=findMesh(data,"FemaleCasualSuit01");
    const auto brows=findMesh(data,"Eyebrows001"),lashes=findMesh(data,"Eyelashes01");
    const auto &hairMaterial=data.materials.at(data.meshes[hair].material);
    const auto &suitMaterial=data.materials.at(data.meshes[suit].material);
    CHECK(hairMaterial.baseColorImage&&hairMaterial.surface.doubleSided&&hairMaterial.surface.alphaMode==Object3DAlphaMode::Blend);
    CHECK(suitMaterial.baseColorImage&&suitMaterial.normalImage&&!suitMaterial.surface.doubleSided&&suitMaterial.surface.alphaMode==Object3DAlphaMode::Opaque);
    CHECK(data.meshes[hair].topology->points.size()==5203&&data.meshes[suit].topology->points.size()==2197);
    for(const auto mesh:{hair,suit,brows,lashes})for(const auto &point:data.meshes[mesh].topology->points)
    {
        double sum=0;for(const auto &weight:point.weights)sum+=weight.weight;CHECK(std::abs(sum-1)<1e-8);
    }
    MakeHumanTemplateOptions unmasked=options;unmasked.accessories->hideCoveredBody=false;
    const auto open=loadMakeHumanTemplate(engine.reference(),base,unmasked).generate();
    const auto &maskedBody=*data.meshes[0].topology,&openBody=*open->data().meshes[0].topology;
    CHECK(maskedBody.points.size()==19158&&maskedBody.layout->definition().polygons==openBody.layout->definition().polygons);
    CHECK(maskedBody.layout->definition().hiddenPolygons.size()>openBody.layout->definition().hiddenPolygons.size());
    CHECK(maskedBody.layout->triangles().size()<openBody.layout->triangles().size());
    const std::array values{TemplateValue{"model/head/head-scale-horiz-decr-incr",.6},TemplateValue{"model/hip/hip-scale-horiz-decr-incr",.6}};
    const auto shaped=human.generate(values);double headMove=0,clothesMove=0;
    for(std::size_t i=0;i<data.meshes[hair].topology->points.size();++i)
        headMove=std::max(headMove,distance(data.meshes[hair].topology->points[i].position,shaped->data().meshes[hair].topology->points[i].position));
    for(std::size_t i=0;i<data.meshes[suit].topology->points.size();++i)
        clothesMove=std::max(clothesMove,distance(data.meshes[suit].topology->points[i].position,shaped->data().meshes[suit].topology->points[i].position));
    CHECK(headMove>1e-4&&clothesMove>1e-4);
    auto editor=human.edit();editor.setTemplateValues(values);editor.moveControlPoints(suit,std::array{TopologyPointMove{0,{.001,0,0}}});
    Object3DEngine restored;auto project=importObject3DProject(restored.reference(),exportObject3DProject(editor.session()));
    CHECK(project.editor.get().object().get().data().meshes.size()==5);
    CHECK(project.editor.get().object().get().data().meshes[0].topology->layout->definition().hiddenPolygons==maskedBody.layout->definition().hiddenPolygons);
    CHECK(project.editor.get().undo()&&project.editor.get().redo());
    std::cout<<"headMove="<<headMove<<" clothesMove="<<clothesMove<<" hidden="<<maskedBody.layout->definition().hiddenPolygons.size()-openBody.layout->definition().hiddenPolygons.size()<<'\n';
}
void failures(const std::filesystem::path &base,const std::filesystem::path &dir)
{
    Object3DEngine engine;const auto initial=engine.objectCount(),templates=engine.reference().templateCount();
    MakeHumanTemplateOptions options;options.accessories=selection(dir);options.accessories->componentIds={"missing"};
    rejects([&]{(void)loadMakeHumanTemplate(engine.reference(),base,options);});
    options.accessories->componentIds={"hair/bob01","hair/bob01"};
    rejects([&]{(void)loadMakeHumanTemplate(engine.reference(),base,options);});
    bool early=false;try{(void)loadMakeHumanTemplate(engine.reference(),base/"__nonexistent__",options);}
    catch(const std::invalid_argument &error){early=std::string_view(error.what()).find("Duplicate human accessory")!=std::string_view::npos;}
    CHECK(early);
    options.accessories->componentIds.clear();rejects([&]{(void)loadMakeHumanTemplate(engine.reference(),base,options);});
    options.accessories=selection(dir);options.maximumBytes=1024;
    rejects([&]{(void)loadMakeHumanTemplate(engine.reference(),base,options);});
    CHECK(engine.objectCount()==initial&&engine.reference().templateCount()==templates);
}
void alternate(const std::filesystem::path &base,const std::filesystem::path &dir)
{
    Object3DEngine engine;MakeHumanTemplateOptions options;
    options.accessories=HumanAccessorySelection{dir,{"hair/short01","clothes/male_casualsuit01"},true};
    options.faceAssets=base.parent_path()/"MakeHumanFace";
    const auto human=loadMakeHumanTemplate(engine.reference(),base,options);const auto asset=human.generate();
    const auto &data=asset->data();CHECK(data.meshes.size()==6&&data.images.size()==6);
    CHECK(human.program().get().definition().surfaces.size()==5);
    const auto hair=findMesh(data,"HairShort01"),suit=findMesh(data,"MaleCasualSuit01");
    CHECK(data.meshes[hair].topology->points.size()==2984&&data.meshes[suit].topology->points.size()==8426);
    CHECK(data.materials[data.meshes[hair].material].surface.doubleSided);
    CHECK(data.materials[data.meshes[suit].material].normalImage.has_value());
    for(const auto mesh:{hair,suit})for(const auto &point:data.meshes[mesh].topology->points)
    {double sum=0;for(const auto weight:point.weights)sum+=weight.weight;CHECK(std::abs(sum-1)<1e-8);}
}
void editing(const std::filesystem::path &base,const std::filesystem::path &dir,bool fullHistory)
{
    Object3DEngine engine;MakeHumanTemplateOptions options;options.accessories=selection(dir);
    const auto original=loadMakeHumanTemplate(engine.reference(),base,options);
    options.accessories->componentIds[0]="hair/short01";
    const auto shortHair=loadMakeHumanTemplate(engine.reference(),base,options);
    options.accessories->componentIds[1]="clothes/male_casualsuit01";
    const auto maleOutfit=loadMakeHumanTemplate(engine.reference(),base,options);
    auto editor=original.edit({},fullHistory?32:4);const std::array values{TemplateValue{"model/head/head-scale-horiz-decr-incr",.5}};
    editor.setTemplateValues(values);editor.moveControlPoints(0,std::array{TopologyPointMove{5000,{.001,0,0}}});
    auto surface=editor.asset()->data().materials[0].surface;surface.doubleSided=true;
    CHECK(editor.setMaterialSurface(0,surface));
    HumanSkinSelection skin;skin.directory=base.parent_path()/"MakeHumanSkin";skin.preset="young_african_male";
    CHECK(setHumanSkin(editor,skin));const auto skinImage=editor.asset()->data().images[*editor.asset()->data().materials[0].baseColorImage];
    const auto &bindings=shortHair.program().get().definition().surfaces;
    const auto hairBinding=std::find_if(bindings.begin(),bindings.end(),[](const auto &binding){return binding.targetMesh==1;});
    CHECK(hairBinding!=bindings.end()&&!hairBinding->points.empty());const auto hairPoint=hairBinding->points.front();
    const auto head=editor.asset()->joint("head");CHECK(head!=NoBone);std::vector<TopologyPointWeights> editedWeights;
    for(const auto source:hairPoint.vertices)
        if(std::none_of(editedWeights.begin(),editedWeights.end(),[&](const auto &row){return row.point==source;}))
            editedWeights.push_back({source,{{head,1}}});
    editor.setControlWeights(0,editedWeights);
    std::vector<double> weights(editor.asset()->data().meshes[0].topology->points.size(),0);weights[5000]=1;
    editor.setPointSelection("Body detail",std::array{MeshPointMask{0,weights}});
    const auto originalBody=editor.asset()->data().meshes[0].topology->points[5000].position;
    const auto oldHidden=editor.asset()->data().meshes[0].topology->layout->definition().hiddenPolygons;
    editor.moveControlPoints(1,std::array{TopologyPointMove{0,{0,.001,0}}});const auto revision=editor.revision();
    rejects([&]{(void)setHumanAccessories(editor,shortHair,dir);});CHECK(editor.revision()==revision);
    CHECK(editor.undo());CHECK(setHumanAccessories(editor,shortHair,dir));
    CHECK(editor.asset()->data().meshes[1].name=="HairShort01");
    const auto &newWeights=editor.asset()->data().meshes[1].topology->points[hairPoint.point].weights;
    CHECK(newWeights.size()==1&&newWeights[0].bone==head&&std::abs(newWeights[0].weight-1)<1e-10);
    CHECK(editor.asset()->data().materials[0].surface.doubleSided&&editor.pointSelection("Body detail")[0].weights[5000]==1);
    CHECK(editor.asset()->data().images[*editor.asset()->data().materials[0].baseColorImage]==skinImage);
    CHECK(distance(editor.asset()->data().meshes[0].topology->points[5000].position,originalBody)<1e-10);
    const auto unchanged=editor.revision();CHECK(!setHumanAccessories(editor,shortHair,dir)&&editor.revision()==unchanged);
    CHECK(editor.undo()&&editor.asset()->data().meshes[1].name=="HairBob01");
    CHECK(editor.redo()&&editor.asset()->data().meshes[1].name=="HairShort01");
    CHECK(setHumanAccessories(editor,maleOutfit,dir));
    CHECK(editor.asset()->data().meshes[2].name=="MaleCasualSuit01");
    CHECK(editor.asset()->data().meshes[0].topology->layout->definition().hiddenPolygons!=oldHidden);
    CHECK(distance(editor.asset()->data().meshes[0].topology->points[5000].position,originalBody)<1e-10);
    CHECK(editor.asset()->data().materials[0].surface.doubleSided&&editor.pointSelection("Body detail")[0].weights[5000]==1);
    CHECK(editor.asset()->data().images[*editor.asset()->data().materials[0].baseColorImage]==skinImage);
    Object3DProjectOptions projectOptions;
    if(fullHistory)
    {
        const auto revision=editor.revision(),objectId=editor.asset()->object().id();const auto objects=engine.objectCount();
        bool exceeded=false;try{(void)exportObject3DProject(editor.session());}
        catch(const std::length_error &error){exceeded=std::string_view(error.what()).find("Project byte budget")!=std::string_view::npos;}
        CHECK(exceeded&&editor.revision()==revision&&editor.asset()->object().id()==objectId&&engine.objectCount()==objects);
        projectOptions.maximumBytes=768ULL*1024*1024;projectOptions.maximumDecodedBytes=1536ULL*1024*1024;
        projectOptions.maximumElements=64'000'000;
    }
    const auto bytes=exportObject3DProject(editor.session(),{},projectOptions);std::cout<<"projectBytes="<<bytes.size()<<'\n';
    Object3DEngine destination;auto project=importObject3DProject(destination.reference(),bytes,projectOptions);
    CHECK(project.editor.get().object().get().data().meshes[2].name=="MaleCasualSuit01");
    CHECK(project.editor.get().undo()&&project.editor.get().object().get().data().meshes[2].name=="FemaleCasualSuit01");
    CHECK(project.editor.get().redo()&&project.editor.get().object().get().data().meshes[2].name=="MaleCasualSuit01");
    project.editor.get().setTemplateValues(std::array{TemplateValue{"model/head/head-scale-horiz-decr-incr",.25}});
}
void batch(const std::filesystem::path &base,const std::filesystem::path &dir)
{
    Object3DEngine engine;MakeHumanTemplateOptions options;options.accessories=selection(dir);
    const auto original=loadMakeHumanTemplate(engine.reference(),base,options);
    options.accessories->componentIds[0]="hair/short01";options.accessories->componentIds[1]="clothes/male_casualsuit01";
    const auto alternative=loadMakeHumanTemplate(engine.reference(),base,options);auto editor=original.edit();
    editor.moveControlPoints(0,std::array{TopologyPointMove{5000,{.002,0,0}}});const auto before=editor.revision();
    CHECK(setHumanAccessories(editor,alternative,dir)&&editor.revision()==before+1);
    CHECK(editor.asset()->data().meshes[1].name=="HairShort01"&&editor.asset()->data().meshes[2].name=="MaleCasualSuit01");
    CHECK(editor.undo()&&editor.asset()->data().meshes[1].name=="HairBob01"&&editor.asset()->data().meshes[2].name=="FemaleCasualSuit01");
    CHECK(editor.redo());
    options.accessories->componentIds.pop_back();const auto fewer=loadMakeHumanTemplate(engine.reference(),base,options);
    const auto revision=editor.revision();CHECK(setHumanAccessories(editor,fewer,dir));CHECK(editor.revision()==revision+1);
    CHECK(editor.asset()->data().meshes.size()==4&&editor.undo()&&editor.asset()->data().meshes.size()==5);
}
void visibility(const std::filesystem::path &base,const std::filesystem::path &dir)
{
    Object3DEngine engine;MakeHumanTemplateOptions options;options.accessories=selection(dir);
    const auto original=loadMakeHumanTemplate(engine.reference(),base,options);options.accessories->hideCoveredBody=false;
    const auto uncovered=loadMakeHumanTemplate(engine.reference(),base,options);auto editor=original.edit();
    editor.moveControlPoints(2,std::array{TopologyPointMove{0,{0,.002,0}}});
    std::vector<double> weights(editor.asset()->data().meshes[2].topology->points.size(),0);weights[0]=1;
    editor.setPointSelection("Garment point",std::array{MeshPointMask{2,weights}});
    const auto position=editor.asset()->data().meshes[2].topology->points[0].position;
    const auto hidden=editor.asset()->data().meshes[0].topology->layout->definition().hiddenPolygons.size();
    CHECK(setHumanAccessories(editor,uncovered,dir));
    CHECK(editor.asset()->data().meshes[0].topology->layout->definition().hiddenPolygons.size()<hidden);
    CHECK(distance(editor.asset()->data().meshes[2].topology->points[0].position,position)<1e-10);
    CHECK(editor.pointSelection("Garment point")[0].weights[0]==1);
    const auto revision=editor.revision();CHECK(!setHumanAccessories(editor,uncovered,dir)&&editor.revision()==revision);
    CHECK(editor.undo()&&editor.asset()->data().meshes[0].topology->layout->definition().hiddenPolygons.size()==hidden);
    CHECK(editor.redo()&&distance(editor.asset()->data().meshes[2].topology->points[0].position,position)<1e-10);
}
void removal(const std::filesystem::path &base,const std::filesystem::path &dir)
{
    Object3DEngine engine;MakeHumanTemplateOptions options;options.accessories=selection(dir);
    const auto original=loadMakeHumanTemplate(engine.reference(),base,options);
    options.accessories->componentIds.erase(options.accessories->componentIds.begin()+1);
    const auto withoutSuit=loadMakeHumanTemplate(engine.reference(),base,options);
    auto editor=original.edit({},4);editor.setTemplateValues(std::array{TemplateValue{"model/head/head-scale-horiz-decr-incr",.4}});
    editor.moveControlPoints(0,std::array{TopologyPointMove{5000,{.001,0,0}}});
    editor.moveControlPoints(3,std::array{TopologyPointMove{0,{0,.002,0}}});
    HumanSkinSelection skin;skin.directory=base.parent_path()/"MakeHumanSkin";skin.preset="young_african_male";
    CHECK(setHumanSkin(editor,skin));const auto skinImage=editor.asset()->data().images[*editor.asset()->data().materials[0].baseColorImage];
    std::vector<double> browWeights(editor.asset()->data().meshes[3].topology->points.size());browWeights[0]=1;
    editor.setPointSelection("Brow",std::array{MeshPointMask{3,browWeights}});
    const auto browPosition=editor.asset()->data().meshes[3].topology->points[0].position;
    const auto bodyPosition=editor.asset()->data().meshes[0].topology->points[5000].position;
    const auto previousHidden=editor.asset()->data().meshes[0].topology->layout->definition().hiddenPolygons.size();
    CHECK(setHumanAccessories(editor,withoutSuit,dir));
    CHECK(editor.asset()->data().meshes.size()==4&&editor.asset()->data().meshes[2].name=="Eyebrows001");
    CHECK(editor.pointSelection("Brow")[0].mesh==2&&editor.pointSelection("Brow")[0].weights[0]==1);
    CHECK(distance(editor.asset()->data().meshes[2].topology->points[0].position,browPosition)<1e-10);
    CHECK(distance(editor.asset()->data().meshes[0].topology->points[5000].position,bodyPosition)<1e-10);
    CHECK(editor.asset()->data().images[*editor.asset()->data().materials[0].baseColorImage]==skinImage);
    CHECK(editor.asset()->data().meshes[0].topology->layout->definition().hiddenPolygons.size()<previousHidden);
    Object3DProjectOptions limits;limits.maximumBytes=768ULL*1024*1024;limits.maximumDecodedBytes=1536ULL*1024*1024;limits.maximumElements=64'000'000;
    const auto bytes=exportObject3DProject(editor.session(),{},limits);CHECK(bytes[8]==33);
    Object3DEngine destination;auto project=importObject3DProject(destination.reference(),bytes,limits);
    CHECK(project.editor.get().undo()&&project.editor.get().pointSelection("Brow")[0].mesh==3);
    CHECK(project.editor.get().redo()&&project.editor.get().pointSelection("Brow")[0].mesh==2);
    project.editor.get().setTemplateValues(std::array{TemplateValue{"model/head/head-scale-horiz-decr-incr",.2}});
    auto editedSuit=original.edit();editedSuit.moveControlPoints(2,std::array{TopologyPointMove{0,{0,.003,0}}});
    const auto revision=editedSuit.revision();rejects([&]{(void)setHumanAccessories(editedSuit,withoutSuit,dir);});CHECK(editedSuit.revision()==revision);
    CHECK(setHumanAccessories(editedSuit,withoutSuit,dir,TemplateSurfaceRemovalOptions{true}));
    CHECK(editedSuit.undo()&&editedSuit.asset()->data().meshes[2].name=="FemaleCasualSuit01");
    options.accessories.reset();const auto bare=loadMakeHumanTemplate(engine.reference(),base,options);auto clean=original.edit();
    CHECK(setHumanAccessories(clean,bare,dir)&&clean.asset()->data().meshes.size()==1);
    CHECK(clean.undo()&&clean.asset()->data().meshes.size()==5);
}
void addition(const std::filesystem::path &base,const std::filesystem::path &dir)
{
    Object3DEngine engine;MakeHumanTemplateOptions options;options.accessories=selection(dir);
    const auto full=loadMakeHumanTemplate(engine.reference(),base,options);
    options.accessories->componentIds.erase(options.accessories->componentIds.begin()+1);
    const auto withoutSuit=loadMakeHumanTemplate(engine.reference(),base,options);
    auto editor=withoutSuit.edit({},5);editor.setTemplateValues(std::array{TemplateValue{"model/head/head-scale-horiz-decr-incr",.4}});
    editor.moveControlPoints(0,std::array{TopologyPointMove{5000,{.001,0,0}}});
    editor.moveControlPoints(2,std::array{TopologyPointMove{0,{0,.002,0}}});
    const auto browLayer=editor.layers().back().id;
    std::vector<double> browMask(editor.asset()->data().meshes[2].topology->points.size());browMask[0]=1;
    editor.setLayerPointMasks(browLayer,std::array{MeshPointMask{2,browMask}});
    const auto originalBodyMaterial=editor.asset()->data().materials[0];
    HumanSkinSelection skin;skin.directory=base.parent_path()/"MakeHumanSkin";skin.preset="young_african_male";
    CHECK(setHumanSkin(editor,skin));const auto skinLayer=editor.layers().back().id;
    const auto skinImage=editor.asset()->data().images[*editor.asset()->data().materials[0].baseColorImage];
    const auto &bindings=full.program().get().definition().surfaces;
    const auto suitBinding=std::find_if(bindings.begin(),bindings.end(),[](const auto &binding){return binding.targetMesh==2;});
    CHECK(suitBinding!=bindings.end()&&!suitBinding->points.empty());const auto suitPoint=suitBinding->points.front();
    const auto head=editor.asset()->joint("head");CHECK(head!=NoBone);std::vector<TopologyPointWeights> weights;
    for(const auto source:suitPoint.vertices)
        if(std::none_of(weights.begin(),weights.end(),[&](const auto &row){return row.point==source;}))weights.push_back({source,{{head,1}}});
    editor.setControlWeights(0,weights);editor.setPointSelection("Brow",std::array{MeshPointMask{2,browMask}});
    const auto browPosition=editor.asset()->data().meshes[2].topology->points[0].position;
    const auto bodyPosition=editor.asset()->data().meshes[0].topology->points[5000].position;
    const auto hidden=editor.asset()->data().meshes[0].topology->layout->definition().hiddenPolygons.size();
    CHECK(setHumanAccessories(editor,full,dir));
    const auto &data=editor.asset()->data();CHECK(data.meshes.size()==5&&data.meshes[2].name=="FemaleCasualSuit01"&&data.meshes[3].name=="Eyebrows001");
    CHECK(editor.pointSelection("Brow")[0].mesh==3&&editor.layerPointMasks(browLayer)[0].mesh==3);
    CHECK(distance(data.meshes[3].topology->points[0].position,browPosition)<1e-10);
    CHECK(distance(data.meshes[0].topology->points[5000].position,bodyPosition)<1e-10);
    CHECK(data.images[*data.materials[0].baseColorImage]==skinImage);
    CHECK(data.meshes[0].topology->layout->definition().hiddenPolygons.size()>hidden);
    const auto &addedWeights=data.meshes[2].topology->points[suitPoint.point].weights;
    CHECK(addedWeights.size()==1&&addedWeights[0].bone==head&&std::abs(addedWeights[0].weight-1)<1e-10);
    const auto &material=data.materials[data.meshes[2].material];CHECK(material.baseColorImage&&material.normalImage);
    const auto suitImage=data.images[*material.baseColorImage],normalImage=data.images[*material.normalImage];
    Object3DProjectOptions limits;limits.maximumBytes=768ULL*1024*1024;limits.maximumDecodedBytes=1536ULL*1024*1024;limits.maximumElements=64'000'000;
    const auto bytes=exportObject3DProject(editor.session(),{},limits);CHECK(bytes[8]==34);
    Object3DEngine destination;auto project=importObject3DProject(destination.reference(),bytes,limits);
    CHECK(project.editor.get().undo()&&project.editor.get().pointSelection("Brow")[0].mesh==2);
    CHECK(project.editor.get().redo()&&project.editor.get().pointSelection("Brow")[0].mesh==3);
    project.editor.get().setLayerEnabled(skinLayer,false);const auto &withoutSkin=project.editor.get().object().get().data();
    CHECK(withoutSkin.materials[0].baseColorImage==originalBodyMaterial.baseColorImage);
    const auto &suitWithoutSkin=withoutSkin.materials[withoutSkin.meshes[2].material];
    CHECK(withoutSkin.images[*suitWithoutSkin.baseColorImage]==suitImage&&withoutSkin.images[*suitWithoutSkin.normalImage]==normalImage);
    project.editor.get().setLayerEnabled(skinLayer,true);const auto &withSkin=project.editor.get().object().get().data();
    CHECK(withSkin.images[*withSkin.materials[0].baseColorImage]==skinImage);
    auto maleOptions=options;maleOptions.accessories->componentIds.insert(maleOptions.accessories->componentIds.begin()+1,"clothes/male_casualsuit01");
    const auto male=loadMakeHumanTemplate(engine.reference(),base,maleOptions);
    CHECK(setHumanAccessories(editor,male,dir));CHECK(editor.asset()->data().meshes[2].name=="MaleCasualSuit01");
    CHECK(editor.asset()->data().images[*editor.asset()->data().materials[0].baseColorImage]==skinImage);
    CHECK(editor.pointSelection("Brow")[0].mesh==3&&editor.undo()&&editor.asset()->data().meshes[2].name=="FemaleCasualSuit01");
    CHECK(setHumanAccessories(editor,withoutSuit,dir)&&setHumanAccessories(editor,full,dir));
    CHECK(editor.asset()->data().images[*editor.asset()->data().materials[0].baseColorImage]==skinImage);
    const auto suit=editor.asset()->data().materials[editor.asset()->data().meshes[2].material];
    CHECK(suit.baseColorImage&&suit.normalImage&&editor.asset()->data().images[*suit.baseColorImage]==suitImage);
    CHECK(setHumanAccessories(editor,withoutSuit,dir));auto changed=editor.asset()->data();
    const auto oldImage=std::find_if(changed.images.begin(),changed.images.end(),[&](const auto &image){return image.name()==suitImage.name();});
    CHECK(oldImage!=changed.images.end());const auto replacementBytes=changed.images.front().encoded();
    *oldImage=Object3DImage(std::string(suitImage.name()),std::vector<std::uint8_t>(replacementBytes.begin(),replacementBytes.end()));
    editor.replace(std::move(changed),"Edit unused image");CHECK(setHumanAccessories(editor,full,dir));
    const auto &conflict=editor.asset()->data();const auto &restoredImage=conflict.images[*conflict.materials[conflict.meshes[2].material].baseColorImage];
    CHECK(restoredImage.name()!=suitImage.name()&&std::equal(restoredImage.encoded().begin(),restoredImage.encoded().end(),suitImage.encoded().begin(),suitImage.encoded().end()));
    options.accessories.reset();const auto bare=loadMakeHumanTemplate(engine.reference(),base,options);auto empty=bare.edit();
    CHECK(setHumanAccessories(empty,full,dir)&&empty.asset()->data().meshes.size()==5);
    for(std::size_t mesh=1;mesh<empty.asset()->data().meshes.size();++mesh)
    {
        const auto &part=empty.asset()->data().meshes[mesh];CHECK(empty.asset()->data().materials[part.material].baseColorImage.has_value());
        for(const auto &point:part.topology->points)
        {double sum=0;for(const auto weight:point.weights)sum+=weight.weight;CHECK(std::abs(sum-1)<1e-8);}
    }
    CHECK(empty.undo()&&empty.asset()->data().meshes.size()==1);
}
void composition(const std::filesystem::path &base,const std::filesystem::path &dir)
{
    Object3DEngine engine;MakeHumanTemplateOptions options;options.accessories=selection(dir);
    const auto original=loadMakeHumanTemplate(engine.reference(),base,options);
    options.accessories->componentIds={"eyebrows/eyebrow001","hair/short01","eyelashes/eyelashes01"};
    const auto alternative=loadMakeHumanTemplate(engine.reference(),base,options);auto editor=original.edit({},6);
    editor.moveControlPoints(0,std::array{TopologyPointMove{5000,{.001,0,0}}});
    editor.moveControlPoints(3,std::array{TopologyPointMove{0,{0,.002,0}}});
    const auto layer=editor.layers().back().id;
    std::vector<double> mask(editor.asset()->data().meshes[3].topology->points.size());mask[0]=1;
    editor.setLayerPointMasks(layer,std::array{MeshPointMask{3,mask}});
    editor.setPointSelection("Brow",std::array{MeshPointMask{3,mask}});
    const auto body=editor.asset()->data().meshes[0].topology->points[5000].position;
    const auto brow=editor.asset()->data().meshes[3].topology->points[0].position;
    const auto revision=editor.revision();CHECK(setHumanAccessories(editor,alternative,dir));
    const auto &data=editor.asset()->data();
    CHECK(editor.revision()==revision+1&&data.meshes.size()==4);
    CHECK(data.meshes[1].name=="Eyebrows001"&&data.meshes[2].name=="HairShort01"&&data.meshes[3].name=="Eyelashes01");
    CHECK(distance(data.meshes[0].topology->points[5000].position,body)<1e-10&&distance(data.meshes[1].topology->points[0].position,brow)<1e-10);
    CHECK(editor.pointSelection("Brow")[0].mesh==1&&editor.layerPointMasks(layer)[0].mesh==1);
    CHECK(data.materials[data.meshes[2].material].baseColorImage.has_value());
    CHECK(editor.undo()&&editor.asset()->data().meshes[3].name=="Eyebrows001"&&editor.pointSelection("Brow")[0].mesh==3);
    CHECK(editor.redo()&&editor.pointSelection("Brow")[0].mesh==1);
    Object3DProjectOptions limits;limits.maximumBytes=768ULL*1024*1024;limits.maximumDecodedBytes=1536ULL*1024*1024;limits.maximumElements=64'000'000;
    const auto bytes=exportObject3DProject(editor.session(),{},limits);CHECK(bytes[8]==35);
    Object3DEngine destination;auto project=importObject3DProject(destination.reference(),bytes,limits);
    CHECK(project.editor.get().undo()&&project.editor.get().pointSelection("Brow")[0].mesh==3);
    CHECK(project.editor.get().redo()&&project.editor.get().pointSelection("Brow")[0].mesh==1);
    auto protectedEditor=original.edit();protectedEditor.moveControlPoints(1,std::array{TopologyPointMove{0,{.001,0,0}}});
    const auto before=protectedEditor.revision();rejects([&]{(void)setHumanAccessories(protectedEditor,alternative,dir);});
    CHECK(protectedEditor.revision()==before);
    CHECK(setHumanAccessories(protectedEditor,alternative,dir,TemplateSurfaceRemovalOptions{true}));
    CHECK(protectedEditor.undo()&&protectedEditor.asset()->data().meshes[1].name=="HairBob01");
}
void retainedSlot(const std::filesystem::path &dir)
{
    Object3DEngine engine;
    const auto make=[&](std::initializer_list<std::string_view> names)
    {
        auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
        data.bones={{"root",NoBone,{}}};for(auto &point:data.meshes[0].topology->points)point.weights={{0,1}};
        std::vector<SurfaceBinding> bindings;
        for(const auto name:names)
        {
            auto part=data.meshes[0];part.name=std::string(name);data.meshes.push_back(std::move(part));
            const auto target=data.meshes.size()-1;
            const std::array rows{SurfaceControlPoint{0,{0,0,0},{1,0,0},{0,0,.1}},SurfaceControlPoint{1,{1,1,1},{1,0,0},{0,0,.1}},SurfaceControlPoint{2,{2,2,2},{1,0,0},{0,0,.1}}};
            bindings.push_back(bindMeshControlPoints(data,0,target,rows));fitMeshSurfaceBinding(data,bindings.back());(void)transferMeshBindingWeights(data,bindings.back());
        }
        Object3DTemplateDefinition definition;definition.base=engine.createObject(std::move(data));definition.surfaces=std::move(bindings);
        return CharacterTemplate(engine.createTemplate(std::move(definition)));
    };
    const auto old=make({"A","B"}),next=make({"B","C"});auto editor=old.edit();
    editor.moveControlPoints(2,std::array{TopologyPointMove{1,{0,0,.03}}});
    editor.setPointSelection("B",std::array{MeshPointMask{2,{0,1,0}}});
    const auto point=editor.asset()->data().meshes[2].topology->points[1].position;
    const auto catalogue=dir.parent_path()/"fixtures/MakeHumanWardrobeComposition";
    CHECK(setHumanAccessories(editor,next,catalogue));
    CHECK(editor.asset()->data().meshes[1].name=="B"&&editor.asset()->data().meshes[2].name=="C");
    CHECK(distance(editor.asset()->data().meshes[1].topology->points[1].position,point)<1e-10);
    CHECK(editor.pointSelection("B")[0].mesh==1&&editor.undo()&&editor.pointSelection("B")[0].mesh==2);
}
void rigFollow(const std::filesystem::path &base,const std::filesystem::path &dir)
{
    Object3DEngine engine;MakeHumanTemplateOptions options;options.accessories=selection(dir);
    options.expressionAssets=base.parent_path()/"MakeHumanExpressions";
    const auto human=loadMakeHumanTemplate(engine.reference(),base,options);auto editor=human.edit({},5);
    const auto initial=human.createFaceRig(*editor.asset());Object3DEditorRigSet follower(editor.session(),std::array{initial});
    CHECK(!initial.get().definition().rawControls.empty());
    const auto name=initial.get().definition().rawControls.front();const std::array controls{NamedRigControl{name,.5}};
    const auto previous=follower.evaluateNamed(0,controls);CHECK(previous.object==editor.asset()->object());
    const auto head=editor.asset()->joint("head");CHECK(head!=NoBone);editor.renameJoint(head,"head_custom");
    const auto renamed=follower.evaluateNamed(0,controls);CHECK(renamed.object==editor.asset()->object());
    CHECK(std::equal(follower.rig(0).get().jointMapping().begin(),follower.rig(0).get().jointMapping().end(),initial.get().jointMapping().begin(),initial.get().jointMapping().end()));
    options.accessories->componentIds={"eyebrows/eyebrow001","hair/short01","eyelashes/eyelashes01"};
    const auto alternative=loadMakeHumanTemplate(engine.reference(),base,options);
    CHECK(setHumanAccessories(editor,alternative,dir));
    const auto automatic=follower.evaluateNamed(0,controls);const auto current=follower.rig(0);
    CHECK(automatic.object==editor.asset()->object()&&current.get().object()==editor.asset()->object());
    const auto manual=human.createFaceRig(*editor.asset(),initial.get().jointMapping());
    const auto reference=manual.get().evaluateNamed(controls);CHECK(automatic.joints==reference.joints&&automatic.blendShapes==reference.blendShapes);
    auto actor=editor.asset()->instantiate();current.get().applyTo(*actor,automatic);
    CHECK(actor->pose().locals().size()==automatic.joints.size());
    Object3DProjectOptions limits;limits.maximumBytes=768ULL*1024*1024;limits.maximumDecodedBytes=1536ULL*1024*1024;limits.maximumElements=64'000'000;
    const auto bytes=exportObject3DProject(editor.session(),follower.rigs(),limits);
    Object3DEngine destination;const auto restored=importObject3DProject(destination.reference(),bytes,limits);
    CHECK(restored.rigs.size()==1&&restored.rigs[0].get().object()==restored.editor.get().object());
    sameTransforms(restored.rigs[0].get().evaluateNamed(controls).joints,automatic.joints);
    CHECK(editor.undo()&&follower.rig(0).get().object()==editor.asset()->object());
    CHECK(editor.undo()&&follower.rig(0).get().object()==editor.asset()->object());
    CHECK(editor.redo()&&follower.rig(0).get().object()==editor.asset()->object());
}
void instanceFollow(const std::filesystem::path &base,const std::filesystem::path &dir)
{
    Object3DEngine engine;MakeHumanTemplateOptions options;options.accessories=selection(dir);
    options.expressionAssets=base.parent_path()/"MakeHumanExpressions";
    const auto human=loadMakeHumanTemplate(engine.reference(),base,options);auto editor=human.edit({},5);
    TopologyMorph detail;detail.name="Detail";detail.deltas={{5000,{0,0,.002}}};editor.setControlMorph(0,std::move(detail));
    const auto original=editor.asset();auto actor=original->instantiate("Editable");
    const auto head=original->joint("head");CHECK(head!=NoBone);auto start=original->data().bones[head].bindTransform,finish=start;
    finish.rotation=Quaternion::fromAxisAngle({0,0,1},.3);
    auto motion=std::make_shared<AnimationClip>(original->object().get().skeleton(),2,std::vector<BoneTrack>{{head,{{0,start},{2,finish}}}});
    actor->animation().play(motion,AnimationWrap::Clamp);actor->animation().seek(.5);actor->animation().setRate(.5);actor->animation().pause();
    auto *body=actor->findComponent<SkeletalMeshComponent>();CHECK(body&&body->name()=="OBJ");
    const auto &targets=body->mesh()->lods()[0].morphTargets;
    const auto found=std::find_if(targets.begin(),targets.end(),[](const auto &m){return m.name=="Detail";});CHECK(found!=targets.end());
    body->setMorphWeight(static_cast<std::size_t>(found-targets.begin()),.65);actor->setPosition({1,2,3});
    const auto face=human.createFaceRig(*original);Object3DEditorRigSet rigs(editor.session(),std::array{face});
    const auto control=face.get().definition().rawControls.front();const std::array faceControls{NamedRigControl{control,.4}};
    face.get().applyTo(*actor,face.get().evaluateNamed(faceControls));
    options.accessories->componentIds={"eyebrows/eyebrow001","hair/short01","eyelashes/eyelashes01"};
    const auto alternative=loadMakeHumanTemplate(engine.reference(),base,options);CHECK(setHumanAccessories(editor,alternative,dir));
    auto moved=transferObject3DEditorInstance(original->object(),*actor,rigs);
    CHECK(moved.actor->position()==actor->position()&&moved.report.copiedAnimation&&moved.report.rigsRebound&&moved.report.matchedMeshes==3);
    CHECK(moved.report.removedMeshes==2&&moved.report.addedMeshes==1&&std::abs(moved.actor->animation().time()-.5)<1e-12);
    CHECK(moved.actor->animation().wrap()==AnimationWrap::Clamp&&std::abs(moved.actor->animation().rate()-.5)<1e-12&&!moved.actor->animation().isPlaying());
    auto *newBody=moved.actor->findComponent<SkeletalMeshComponent>();CHECK(newBody&&newBody->name()=="OBJ");
    const auto &newTargets=newBody->mesh()->lods()[0].morphTargets;
    const auto newDetail=std::find_if(newTargets.begin(),newTargets.end(),[](const auto &m){return m.name=="Detail";});CHECK(newDetail!=newTargets.end());
    CHECK(std::abs(newBody->morphWeights()[static_cast<std::size_t>(newDetail-newTargets.begin())]-.65)<1e-12);
    const auto evaluation=rigs.evaluateNamed(0,faceControls);CHECK(evaluation.object==editor.asset()->object());
    rigs.rig(0).get().applyTo(*moved.actor,evaluation);
    CHECK(actor->pose().skeleton()==original->object().get().skeleton()&&actor->animation().time()==.5);
}
void progressiveSplit(const std::filesystem::path &base,const std::filesystem::path &dir,bool sparse=false)
{
    Object3DEngine engine;MakeHumanTemplateOptions options;
    options.accessories=HumanAccessorySelection{dir,{"hair/bob01"},true};
    const auto human=loadMakeHumanTemplate(engine.reference(),base,options);auto editor=human.edit({},6);
    const auto body=findMesh(editor.asset()->data(),"OBJ"),hair=findMesh(editor.asset()->data(),"HairBob01");
    for(const auto index:{body,hair})
    {
        const auto &topology=*editor.asset()->data().meshes[index].topology;
        const auto &definition=topology.layout->definition();CHECK(!definition.renderToLayout.empty());
        const auto point=definition.layouts[definition.renderToLayout.front()].position;
        TopologyMorph half;half.name="SharedHalf";half.normalDeltas.emplace();
        half.deltas={{point,{0,index==body?.01:.02,0}}};editor.setControlMorph(index,std::move(half));
        if(!sparse||index==body)
        {
            TopologyMorph full;full.name="SharedFull";full.normalDeltas.emplace();
            full.deltas={{point,{0,index==body?.025:.04,0}}};editor.setControlMorph(index,std::move(full));
        }
    }
    const auto original=editor.asset()->object();
    const auto rig=createObject3DProgressiveMorphRig(original,{{{"Shared",0,{{"SharedHalf",.5},{"SharedFull",1}}}}});
    const AnimationClip clip(original.get().skeleton(),1,{},
        {{object3DProgressiveControlCurveName("Shared"),{{0,0},{1,sparse?.75:.5}}}});
    const auto encoded=exportObject3DFbxWithProgressiveMorphs(rig,clip);CHECK(!encoded.bytes.empty());
    Object3DEngine loadedEngine;const auto loaded=importObject3DFbxWithProgressiveMorphsAndAnimations(loadedEngine.reference(),encoded.bytes);
    const auto importedProgram=inspectObject3DProgressiveMorphRig(loaded.rig);
    CHECK(loaded.animations.size()==1&&importedProgram.channels.size()==1);
    if(sparse)CHECK(importedProgram.channels[0].sourceMeshes.empty());
    else CHECK(importedProgram.channels[0].sourceMeshes.size()==2);
    if(sparse)
    {
        const auto &imported=loaded.asset.object.get().data();const auto hairIndex=findMesh(imported,"HairBob01");
        const auto &morphs=imported.meshes[hairIndex].geometry.morphTargets;
        CHECK(std::none_of(morphs.begin(),morphs.end(),[&](const auto &m){return m.name==importedProgram.channels[0].keys[1].target;}));
    }
    auto expected=original.get().instantiate(),actual=loaded.asset.object.get().instantiate();
    std::vector<SkeletalMeshComponent *> left,right;
    for(auto *component:expected->components())if(auto *part=dynamic_cast<SkeletalMeshComponent *>(component))left.push_back(part);
    for(auto *component:actual->components())if(auto *part=dynamic_cast<SkeletalMeshComponent *>(component))right.push_back(part);
    CHECK(left.size()==2&&right.size()==2);
    std::map<std::string,Vector3,std::less<>> before;
    for(auto *part:left)before.emplace(part->name(),part->deformedVertices()[0].position);
    applyObject3DProgressiveAnimationClip(rig,clip,{},1,expected->pose(),left);
    applyObject3DProgressiveAnimationClip(loaded.rig,*loaded.animations[0].clip,loaded.animations[0].morphBindings,1,actual->pose(),right);
    for(auto *part:left)
    {
        const auto found=std::find_if(right.begin(),right.end(),[&](const auto *other){return other->name()==part->name();});
        CHECK(found!=right.end());
        const auto displacement=part->deformedVertices()[0].position.y-before.at(part->name()).y;
        const auto desired=sparse?(part->name()=="OBJ"?.0175:.01):(part->name()=="OBJ"?.01:.02);
        CHECK(std::abs(displacement-desired)<1e-6);
        CHECK(distance(part->deformedVertices()[0].position,(*found)->deformedVertices()[0].position)<1e-6);
    }
}
void grounded(const std::filesystem::path &base,const std::filesystem::path &dir)
{
    Object3DEngine engine;MakeHumanTemplateOptions options;options.accessories=selection(dir);
    options.macroAssets=base.parent_path()/"MakeHumanMacro";
    const auto original=loadMakeHumanTemplate(engine.reference(),base,options);
    options.accessories->componentIds[0]="hair/short01";
    const auto shortHair=loadMakeHumanTemplate(engine.reference(),base,options);
    HumanMacroParameters parameters;parameters.ageYears=38;parameters.height=.25;parameters.bodyMass=.6;
    auto editor=original.edit(makeHumanMacroValues(parameters),4);
    const auto before=editor.asset()->data().meshes[0].topology->points[5000].position;
    CHECK(setHumanAccessories(editor,shortHair,dir));
    CHECK(distance(editor.asset()->data().meshes[0].topology->points[5000].position,before)<1e-10);
    options.accessories->componentIds[1]="clothes/male_casualsuit01";
    const auto outfit=loadMakeHumanTemplate(engine.reference(),base,options);
    CHECK(setHumanAccessories(editor,outfit,dir));
    CHECK(distance(editor.asset()->data().meshes[0].topology->points[5000].position,before)<1e-10);
    parameters.ageYears=12;parameters.height=-.2;editor.setTemplateValues(makeHumanMacroValues(parameters));
    const auto expected=original.generate(makeHumanMacroValues(parameters));
    CHECK(distance(editor.asset()->data().meshes[0].topology->points[5000].position,expected->data().meshes[0].topology->points[5000].position)<1e-10);
    options.accessories->componentIds[0]="hair/bob01";options.accessories->componentIds.erase(options.accessories->componentIds.begin()+1);
    const auto noSuit=loadMakeHumanTemplate(engine.reference(),base,options);
    auto removed=original.edit(makeHumanMacroValues(parameters),4);
    CHECK(setHumanAccessories(removed,noSuit,dir));
    CHECK(distance(removed.asset()->data().meshes[0].topology->points[5000].position,expected->data().meshes[0].topology->points[5000].position)<1e-10);
    parameters.ageYears=35;parameters.height=.3;removed.setTemplateValues(makeHumanMacroValues(parameters));
    const auto expectedAfter=original.generate(makeHumanMacroValues(parameters));
    CHECK(distance(removed.asset()->data().meshes[0].topology->points[5000].position,expectedAfter->data().meshes[0].topology->points[5000].position)<1e-10);
    CHECK(setHumanAccessories(removed,original,dir));
    CHECK(distance(removed.asset()->data().meshes[0].topology->points[5000].position,expectedAfter->data().meshes[0].topology->points[5000].position)<1e-10);
    parameters.ageYears=20;parameters.height=-.1;removed.setTemplateValues(makeHumanMacroValues(parameters));
    const auto expectedAdded=original.generate(makeHumanMacroValues(parameters));
    CHECK(distance(removed.asset()->data().meshes[0].topology->points[5000].position,expectedAdded->data().meshes[0].topology->points[5000].position)<1e-10);
}
void formats(const std::filesystem::path &base,const std::filesystem::path &dir,const std::filesystem::path &output,bool switched,bool removed,bool added,bool mixed)
{
    Object3DEngine engine;MakeHumanTemplateOptions options;options.accessories=selection(dir);
    if(added)options.accessories->componentIds.erase(options.accessories->componentIds.begin()+1);
    const auto human=loadMakeHumanTemplate(engine.reference(),base,options);auto editor=human.edit();
    if(switched)
    {
        options.accessories->componentIds[0]="hair/short01";options.accessories->componentIds[1]="clothes/male_casualsuit01";
        const auto alternative=loadMakeHumanTemplate(engine.reference(),base,options);
        CHECK(setHumanAccessories(editor,alternative,dir));
    }
    if(removed)
    {
        options.accessories->componentIds.erase(options.accessories->componentIds.begin()+1);
        const auto alternative=loadMakeHumanTemplate(engine.reference(),base,options);
        CHECK(setHumanAccessories(editor,alternative,dir));
    }
    if(added)
    {
        options.accessories=selection(dir);const auto alternative=loadMakeHumanTemplate(engine.reference(),base,options);
        CHECK(setHumanAccessories(editor,alternative,dir));
    }
    if(mixed)
    {
        options.accessories->componentIds={"eyebrows/eyebrow001","hair/short01","eyelashes/eyelashes01"};
        const auto alternative=loadMakeHumanTemplate(engine.reference(),base,options);
        CHECK(setHumanAccessories(editor,alternative,dir));
    }
    const auto asset=editor.asset();
    const auto save=[&](std::string_view name,const std::vector<std::uint8_t> &bytes)
    {std::ofstream file(output/name,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));CHECK(file.good());};
    save("character.glb",exportObject3DGlb(asset->object().get()));save("character.fbx",exportObject3DFbx(asset->object().get()));
}
int main(int argc,char **argv)
{
    try{if(argc!=4&&argc!=5)return 2;const std::string_view mode=argv[1];if((mode=="formats"||mode=="switch_formats"||mode=="remove_formats"||mode=="add_formats"||mode=="mixed_formats")&&argc==5)formats(argv[2],argv[3],argv[4],mode=="switch_formats",mode=="remove_formats",mode=="add_formats",mode=="mixed_formats");else if(argc!=4)return 2;else if(mode=="catalog")catalog(argv[3]);else if(mode=="human")human(argv[2],argv[3]);else if(mode=="alternate")alternate(argv[2],argv[3]);else if(mode=="editing")editing(argv[2],argv[3],false);else if(mode=="editing_large")editing(argv[2],argv[3],true);else if(mode=="batch")batch(argv[2],argv[3]);else if(mode=="visibility")visibility(argv[2],argv[3]);else if(mode=="removal")removal(argv[2],argv[3]);else if(mode=="addition")addition(argv[2],argv[3]);else if(mode=="composition")composition(argv[2],argv[3]);else if(mode=="retained_slot")retainedSlot(argv[3]);else if(mode=="rig_follow")rigFollow(argv[2],argv[3]);else if(mode=="instance_follow")instanceFollow(argv[2],argv[3]);else if(mode=="progressive_split")progressiveSplit(argv[2],argv[3]);else if(mode=="progressive_sparse")progressiveSplit(argv[2],argv[3],true);else if(mode=="grounded")grounded(argv[2],argv[3]);else if(mode=="failures")failures(argv[2],argv[3]);else return 2;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
