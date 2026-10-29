#include <iiActorObject/Features.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#endif
#include <array>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>

using namespace iiActorObject;
#define CHECK(x) do { if(!(x))throw std::runtime_error(#x); } while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
bool near(double a,double b){return std::abs(a-b)<1e-10;}

Object3DData input()
{
    return parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 4 4 4\n"
                            "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nf 1/1 2/2 3/3 4/4\n");
}
Object3DTemplateHandle program(Object3DEngine &engine)
{
    auto data=input();auto follower=data.meshes[0];follower.name="Follower";data.meshes.push_back(std::move(follower));
    Object3DTemplateDefinition def;def.base=engine.createObject(std::move(data));
    def.targets={{"height",0,{{2,{0,.2,0}}}}};return engine.createTemplate(std::move(def));
}
const MeshPointMask face{0,{1,.5,0,0,0}};
void fixChecksum(std::vector<std::uint8_t> &bytes)
{
    std::uint32_t crc=0xffffffffu;for(std::size_t i=24;i<bytes.size();++i)
    {crc^=bytes[i];for(unsigned j=0;j<8;++j)crc=(crc>>1)^((crc&1)?0xedb88320u:0u);}
    crc=~crc;for(unsigned i=0;i<4;++i)bytes[20+i]=static_cast<std::uint8_t>(crc>>(8*i));
}
void snapshot()
{
    Object3DEngine engine;auto object=engine.createObject(input());auto handle=engine.editObject(object);auto &editor=handle.get();
    const auto initial=editor.revision();const auto count=engine.objectCount();
    editor.setPointSelection("Face",std::span(&face,1));
    CHECK(editor.object()==object && engine.objectCount()==count && editor.revision()==initial+1);
    CHECK(editor.pointSelectionNames()==std::vector<std::string>{"Face"});
    CHECK(editor.pointSelection("Face").size()==1 && editor.pointSelection("Face")[0]==face);
    const auto revision=editor.revision();editor.setPointSelection("Face",std::span(&face,1));CHECK(editor.revision()==revision);
    MeshBrushSettings settings;settings.falloff=MeshBrushFalloff::Constant;settings.mask=editor.pointSelection("Face")[0].weights;
    const auto result=editor.sculptStroke(0,std::array{SculptStrokeSample{{{.5,.5,0},2,1},{0,0,.1}}},settings);
    CHECK(result.changedPoints==2);const auto &points=editor.object().get().data().meshes[0].topology->points;
    CHECK(near(points[0].position.z,.1)&&near(points[1].position.z,.05)&&near(points[2].position.z,0));
    CHECK(editor.undo() && editor.pointSelection("Face")[0]==face);
    CHECK(editor.undo() && editor.pointSelectionNames().empty());CHECK(editor.redo());CHECK(editor.redo());
    CHECK(editor.pointSelection("Face")[0]==face);
    const auto source=editor.object();FbxExportOptions exportOptions;exportOptions.externalDocument="named-selection";
    const auto baseline=importObject3DFbx(engine.reference(),exportObject3DFbx(source.get(),exportOptions));
    auto changed=baseline.object.get().data();changed.meshes[0].topology->points[0].position.z+=.03;
    const auto external=engine.createObject(std::move(changed));Object3DExternalOptions options;options.document=exportOptions.externalDocument;
    const auto imported=editor.applyExternalEdits(source.get(),{baseline.object.get(),baseline.externalIdentity},{external.get(),baseline.externalIdentity},options);
    CHECK(imported.changed()&&editor.pointSelection("Face")[0]==face);
    CHECK(near(editor.object().get().data().meshes[0].topology->points[0].position.z,.13));
    CHECK(editor.undo()&&editor.pointSelection("Face")[0]==face);CHECK(editor.redo()&&editor.pointSelection("Face")[0]==face);
}
void validation()
{
    Object3DEngine engine;auto editor=engine.editObject(engine.createObject(input()));auto &e=editor.get();
    e.setPointSelection("Face",std::span(&face,1));const auto revision=e.revision();const auto object=e.object();
    rejects([&]{e.setPointSelection("",std::span(&face,1));});
    rejects([&]{e.setPointSelection(std::string("\xff",1),std::span(&face,1));});
    auto bad=face;bad.weights.pop_back();rejects([&]{e.setPointSelection("Bad",std::span(&bad,1));});
    bad=face;bad.weights[0]=std::numeric_limits<double>::quiet_NaN();rejects([&]{e.setPointSelection("Bad",std::span(&bad,1));});
    const std::array duplicates{face,face};rejects([&]{e.setPointSelection("Bad",duplicates);});
    rejects([&]{(void)e.pointSelection("Unknown");});rejects([&]{e.renamePointSelection("Unknown","Other");});
    e.setPointSelection("Empty",{});CHECK(e.pointSelection("Empty").empty());
    rejects([&]{e.renamePointSelection("Face","Empty");});
    rejects([&]{e.removePointSelection("Unknown");});
    e.removePointSelection("Empty");CHECK(e.object()==object && e.revision()==revision+2);
    const auto noChange=e.revision();e.renamePointSelection("Face","Face");CHECK(e.revision()==noChange);
    const MeshPointMask zero{0,{0,0,0,0,0}};e.setPointSelection("Zero",std::span(&zero,1));
    CHECK(e.pointSelection("Zero").empty());const auto emptyRevision=e.revision();e.setPointSelection("Zero",{});CHECK(e.revision()==emptyRevision);
    const std::string korean="\xec\x96\xbc\xea\xb5\xb4";
    auto external=face;e.setPointSelection(korean,std::span(&external,1));external.weights[0]=0;
    CHECK(e.pointSelection(korean)[0]==face);rejects([&]{e.setLayerPointSelection(999,korean);});
    auto multiData=input();auto follower=multiData.meshes[0];follower.name="Follower";multiData.meshes.push_back(std::move(follower));
    Object3DEngine multi;auto multiEditor=multi.editObject(multi.createObject(std::move(multiData)));
    const std::array rows{MeshPointMask{1,{0,0,1,0,0}},face};multiEditor.get().setPointSelection("Both",rows);
    CHECK(multiEditor.get().pointSelection("Both")[0].mesh==0&&multiEditor.get().pointSelection("Both")[1].mesh==1);
    const auto beforeErase=e.revision();CHECK(engine.eraseObject(e.object().id()));
    rejects([&]{(void)e.pointSelectionNames();});rejects([&]{(void)e.pointSelection("Face");});
    rejects([&]{e.renamePointSelection("Face","Face");});rejects([&]{e.renamePointSelection("Face","Other");});
    rejects([&]{e.removePointSelection("Face");});CHECK(e.revision()==beforeErase);

    Object3DEngine bounded;auto limited=bounded.editObject(bounded.createObject(input()),1);
    for(unsigned i=0;i<1024;++i)limited.get().setPointSelection("set-"+std::to_string(i),{});
    const auto boundedRevision=limited.get().revision();rejects([&]{limited.get().setPointSelection("overflow",{});});
    CHECK(limited.get().pointSelectionNames().size()==1024&&limited.get().revision()==boundedRevision);
}
void layered()
{
    Object3DEngine engine;auto handle=engine.editTemplate(program(engine));auto &e=handle.get();
    e.setPointSelection("Face",std::span(&face,1));auto edited=e.object().get().data();
    for(const auto point:{0,1})edited.meshes[0].topology->points[point].position.z+=.1;
    edited.meshes[1].topology->points[0].position.z+=.1;e.replace(std::move(edited));
    const auto layer=e.layers()[0].id;e.setLayerPointSelection(layer,"Face");
    const auto &masked=e.object().get().data().meshes;
    CHECK(near(masked[0].topology->points[0].position.z,.1)&&near(masked[0].topology->points[1].position.z,.05));
    CHECK(near(masked[1].topology->points[0].position.z,0));
    const auto maskedRevision=e.revision();e.setLayerPointSelection(layer,"Face");CHECK(e.revision()==maskedRevision);
    const auto before=e.pointSelection("Face")[0].weights.data();e.renamePointSelection("Face","Face detail");
    CHECK(e.pointSelection("Face detail")[0].weights.data()==before);
    e.setTemplateValues(std::array{TemplateValue{"height",.5}});
    CHECK(e.pointSelection("Face detail")[0]==face);
    const auto bytes=exportObject3DProject(handle);CHECK(bytes[8]==17&&bytes==exportObject3DProject(handle));
    Object3DEngine restored;auto loaded=importObject3DProject(restored.reference(),bytes);auto &read=loaded.editor.get();
    CHECK(read.pointSelection("Face detail")[0]==face && near(read.object().get().data().meshes[0].topology->points[0].position.z,.1));
    CHECK(read.undo() && read.pointSelection("Face detail")[0]==face);
    CHECK(read.undo() && read.pointSelection("Face")[0]==face);
    CHECK(read.redo() && read.redo());read.detachTemplate();CHECK(read.pointSelection("Face detail")[0]==face);
}
void transition()
{
    Object3DEngine engine;auto editor=engine.editObject(engine.createObject(input()));auto &e=editor.get();
    e.setPointSelection("Face",std::span(&face,1));const auto old=e.object();const auto revision=e.revision();
    auto bad=old.get().data();auto &topology=*bad.meshes[0].topology;topology.points.pop_back();
    auto definition=topology.layout->definition();definition.positionCount=4;
    topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);
    rejects([&]{e.replace(std::move(bad));});CHECK(e.object()==old&&e.revision()==revision&&e.pointSelection("Face")[0]==face);
    bad=old.get().data();auto &reversed=*bad.meshes[0].topology;auto reversedDefinition=reversed.layout->definition();
    std::reverse(reversedDefinition.polygons[0].begin(),reversedDefinition.polygons[0].end());
    reversed.layout=MeshTopologyLayout::create(std::move(reversedDefinition),reversed.points);
    rejects([&]{e.replace(std::move(bad));});CHECK(e.object()==old&&e.revision()==revision);
    auto change=old.get().data();change.meshes[0].name="Renamed";auto &t=*change.meshes[0].topology;auto uv=t.layout->definition();
    std::swap(uv.layouts[0].textureCoordinate,uv.layouts[1].textureCoordinate);
    t.layout=MeshTopologyLayout::create(std::move(uv),t.points);e.replace(std::move(change));CHECK(e.pointSelection("Face")[0]==face);
    e.removePointSelection("Face");CHECK(e.pointSelectionNames().empty());

    auto render=old.get().data();render.meshes[0].topology.reset();auto renderEditor=engine.editObject(engine.createObject(std::move(render)));
    const MeshPointMask renderMask{0,{1,.5,0,0}};renderEditor.get().setPointSelection("Rendered",std::span(&renderMask,1));
    auto replaced=renderEditor.get().object().get().data();std::swap(replaced.meshes[0].geometry.triangles[0],replaced.meshes[0].geometry.triangles[1]);
    const auto before=renderEditor.get().revision();rejects([&]{renderEditor.get().replace(std::move(replaced));});
    CHECK(renderEditor.get().revision()==before&&renderEditor.get().pointSelection("Rendered")[0]==renderMask);
}
void compatibility()
{
    Object3DEngine engine;auto e=engine.editTemplate(program(engine));const auto old=exportObject3DProject(e);CHECK(old[8]==8);
    e.get().setPointSelection("Face",std::span(&face,1));e.get().removePointSelection("Face");
    const auto retained=exportObject3DProject(e);CHECK(retained[8]==17);
    Object3DEngine target;auto loaded=importObject3DProject(target.reference(),retained);CHECK(loaded.editor.get().pointSelectionNames().empty());
    CHECK(loaded.editor.get().undo() && loaded.editor.get().pointSelection("Face")[0]==face);
    CHECK(importObject3DProject(target.reference(),old).editor.get().pointSelectionNames().empty());
    auto shortHistory=engine.editTemplate(program(engine),{},1);shortHistory.get().setPointSelection("Face",std::span(&face,1));
    shortHistory.get().removePointSelection("Face");shortHistory.get().moveControlPoints(0,std::array{TopologyPointMove{0,{0,0,.01}}});
    CHECK(exportObject3DProject(shortHistory)[8]==8);
#if IIACTOROBJECT_HAS_NSCHARACTER
    iiActorObject::NSCharacter::CharacterEditor character(e);character.setPointSelection("Face",std::span(&face,1));
    CHECK(character.pointSelection("Face")[0]==face && &character.session().get()==&e.get());
    std::optional<iiActorObject::NSCharacter::CharacterEditor> expired;
    {
        Object3DEngine owner;auto session=owner.editObject(owner.createObject(input()));
        expired.emplace(session);expired->setPointSelection("Face",std::span(&face,1));CHECK(expired->session().valid());
    }
    CHECK(!expired->session().valid());rejects([&]{(void)expired->pointSelection("Face");});
#endif
}
void corruption()
{
    Object3DEngine source;auto handle=source.editObject(source.createObject(input()),1);auto &e=handle.get();
    e.setPointSelection("LeftArm1",std::span(&face,1));e.setPointSelection("RightArm",std::span(&face,1));
    e.renamePointSelection("LeftArm1","LeftArm2");auto bytes=exportObject3DProject(handle);CHECK(bytes[8]==17);
    Object3DEngine valid;CHECK(importObject3DProject(valid.reference(),bytes).editor.valid());
    const std::string_view old="LeftArm1",replacement="RightArm";
    const auto it=std::search(bytes.begin()+24,bytes.end(),old.begin(),old.end());CHECK(it!=bytes.end());
    std::copy(replacement.begin(),replacement.end(),it);
    fixChecksum(bytes);
    Object3DEngine target;auto guard=target.createObject(input());auto existing=target.editObject(guard);const auto revision=existing.get().revision();
    bool caught=false;try{(void)importObject3DProject(target.reference(),bytes);}
    catch(const std::invalid_argument &error){CHECK(std::string_view(error.what()).find("point selection name")!=std::string_view::npos);caught=true;}
    CHECK(caught);
    CHECK(guard.valid()&&existing.valid()&&existing.get().revision()==revision&&target.objectCount()==1&&target.editorCount()==1);
    CHECK(target.createObject(input()).id()==guard.id()+1);

    auto data=input();auto &topology=*data.meshes[0].topology;auto definition=topology.layout->definition();
    definition.sourceFormat="NamedSelectionSource";definition.sourceElement=712;
    topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);
    Object3DEngine other;auto edit=other.editObject(other.createObject(std::move(data)));
    edit.get().setPointSelection("Face",std::span(&face,1));auto changed=edit.get().object().get().data();
    auto &changedTopology=*changed.meshes[0].topology;auto sameDefinition=changedTopology.layout->definition();
    changedTopology.layout=MeshTopologyLayout::create(std::move(sameDefinition),changedTopology.points);
    changedTopology.points[0].position.z=.01;edit.get().replace(std::move(changed));
    auto changedBytes=exportObject3DProject(edit);const std::string_view marker="NamedSelectionSource";
    const auto first=std::search(changedBytes.begin()+24,changedBytes.end(),marker.begin(),marker.end());CHECK(first!=changedBytes.end());
    const auto second=std::search(first+marker.size(),changedBytes.end(),marker.begin(),marker.end());CHECK(second!=changedBytes.end());
    const auto index=static_cast<std::size_t>(second-changedBytes.begin())+marker.size();
    CHECK(index+8<=changedBytes.size()&&changedBytes[index]==static_cast<std::uint8_t>(712&255));
    const std::uint64_t different=713;for(unsigned i=0;i<8;++i)changedBytes[index+i]=static_cast<std::uint8_t>(different>>(8*i));
    fixChecksum(changedBytes);Object3DEngine destination;auto survivor=destination.createObject(input());caught=false;
    try{(void)importObject3DProject(destination.reference(),changedBytes);}
    catch(const std::invalid_argument &error){CHECK(std::string_view(error.what()).find("stable mesh and source point IDs")!=std::string_view::npos);caught=true;}
    CHECK(caught&&survivor.valid()&&destination.objectCount()==1&&destination.editorCount()==0);
}
int main(int argc,char **argv)
{
    try
    {
        if(argc<2)return 2;const std::string_view mode=argv[1];
        if(mode=="fixture"&&argc==3)
        {
            Object3DEngine engine;auto editor=engine.editTemplate(program(engine));
            editor.get().setPointSelection("Face",std::span(&face,1));const auto bytes=exportObject3DProject(editor);
            CHECK(bytes[8]==17);const std::filesystem::path path=argv[2];std::filesystem::create_directories(path.parent_path());
            std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(file.good());return 0;
        }
        if(argc!=2)return 2;
        if(mode=="snapshot")snapshot();else if(mode=="validation")validation();else if(mode=="layered")layered();
        else if(mode=="transition")transition();else if(mode=="compatibility")compatibility();else if(mode=="corruption")corruption();else return 2;
        std::cout<<mode<<" passed\n";return 0;
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
