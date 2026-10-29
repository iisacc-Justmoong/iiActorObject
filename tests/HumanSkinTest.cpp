#include <iiActorObject/NSCharacter/HumanSkin.h>
#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string_view>

using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
#define CHECK(x) do { if (!(x)) throw std::runtime_error(#x); } while(false)
template<class F> void rejects(F f){bool failed=false;try{f();}catch(const std::exception &){failed=true;}CHECK(failed);}
HumanSkinSelection selection(const std::filesystem::path &directory,std::string preset)
{HumanSkinSelection result;result.directory=directory;result.preset=std::move(preset);return result;}
std::shared_ptr<const CharacterAsset> fixture(Object3DEngine &engine,bool uv=true)
{
    const auto withUv="v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nf 1/1 2/2 3/3\n";
    const auto withoutUv="v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    return std::make_shared<CharacterAsset>(engine.createObject(parseObject3DObj(uv?withUv:withoutUv)));
}
const Object3DImage &image(const CharacterAsset &asset)
{
    const auto &data=asset.data();const auto material=data.meshes.at(0).material;
    const auto selected=data.materials.at(material).baseColorImage;CHECK(selected.has_value());return data.images.at(*selected);
}
void catalog(const std::filesystem::path &directory)
{
    const auto presets=listHumanSkinPresets(directory);CHECK(presets.size()==20);
    CHECK(std::is_sorted(presets.begin(),presets.end(),[](const auto &a,const auto &b){return a.id<b.id;}));
    CHECK(std::any_of(presets.begin(),presets.end(),[](const auto &v){return v.id=="young_african_male";}));
    CHECK(std::all_of(presets.begin(),presets.end(),[](const auto &v){return !v.id.empty()&&!v.label.empty();}));
    rejects([&]{(void)listHumanSkinPresets(directory,2);});
    Object3DEngine engine;CharacterEditor editor(fixture(engine),1);
    for(const auto &preset:presets)
    {
        CHECK(setHumanSkin(editor,selection(directory,preset.id)));
        CHECK(image(*editor.asset()).width()==2048&&image(*editor.asset()).height()==2048);
        CHECK(editor.asset()->data().images.size()==1);
    }
}
void editing(const std::filesystem::path &directory)
{
    Object3DEngine engine;CharacterEditor editor(fixture(engine));const auto a=selection(directory,"young_african_male");
    const auto b=selection(directory,"young_asian_female");CHECK(setHumanSkin(editor,a));
    const auto first=editor.asset();CHECK(image(*first).width()>512&&image(*first).height()>512);
    const auto firstId=image(*first).name();const auto revision=editor.revision();CHECK(!setHumanSkin(editor,a));CHECK(editor.revision()==revision);
    CHECK(setHumanSkin(editor,b));const auto second=editor.asset();CHECK(image(*second).name()!=firstId);
    CHECK(second->data().images.size()==1);CHECK(editor.undo());CHECK(image(*editor.asset()).name()==firstId);
    CHECK(editor.redo());CHECK(image(*editor.asset())==image(*second));
    const auto bytes=exportObject3DProject(editor.session());Object3DEngine restored;
    auto imported=importObject3DProject(restored.reference(),bytes);CharacterEditor copy(imported.editor);
    CHECK(image(*copy.asset())==image(*second));
    const auto glb=exportObject3DGlb(copy.asset()->object().get());
    const auto glbCopy=importObject3DGlb(restored.reference(),glb);const auto glbData=glbCopy.object.get().data();
    CHECK(glbData.images.size()==1&&glbData.images[0].encoded().size()==image(*second).encoded().size());
    CHECK(std::equal(glbData.images[0].encoded().begin(),glbData.images[0].encoded().end(),image(*second).encoded().begin()));
    const auto fbx=exportObject3DFbx(copy.asset()->object().get());const auto fbxCopy=importObject3DFbx(restored.reference(),fbx);
    CHECK(fbxCopy.object.get().data().images.size()==1);
    CHECK(fbxCopy.object.get().data().images[0].encoded().size()==image(*second).encoded().size());
    CHECK(std::equal(fbxCopy.object.get().data().images[0].encoded().begin(),fbxCopy.object.get().data().images[0].encoded().end(),image(*second).encoded().begin()));
}
void templateSkin(const std::filesystem::path &base,const std::filesystem::path &directory)
{
    Object3DEngine engine;MakeHumanTemplateOptions options;
    options.faceAssets=base.parent_path()/"MakeHumanFace";
    options.macroAssets=base.parent_path()/"MakeHumanMacro";
    options.expressionAssets=base.parent_path()/"MakeHumanExpressions";
    const auto a=selection(directory,"young_african_male"),b=selection(directory,"young_asian_female");
    auto human=loadMakeHumanTemplate(engine.reference(),base,options,a);
    auto editor=human.edit();CHECK(image(*editor.asset()).width()>512&&editor.asset()->data().images.size()==4);
    const auto eye=editor.asset()->data().images[0],teeth=editor.asset()->data().images[1],tongue=editor.asset()->data().images[2];
    CHECK(setHumanSkin(editor,b));CHECK(editor.asset()->data().images.size()==4);
    CHECK(editor.asset()->data().images[0]==eye&&editor.asset()->data().images[1]==teeth&&editor.asset()->data().images[2]==tongue);
    const auto before=image(*editor.asset());const auto &targets=human.program().get().definition().targets;
    CHECK(!targets.empty());const std::array values{TemplateValue{targets.front().name,.2}};
    editor.setTemplateValues(values);CHECK(image(*editor.asset())==before);
    CHECK(editor.undo());CHECK(image(*editor.asset())==before);CHECK(editor.undo());CHECK(image(*editor.asset()).name()!=before.name());
    CHECK(editor.redo());CHECK(image(*editor.asset())==before);
    const auto bytes=exportObject3DProject(editor.session());Object3DEngine restored;
    auto saved=importObject3DProject(restored.reference(),bytes);CharacterEditor copy(saved.editor);
    CHECK(copy.layered()&&image(*copy.asset())==before);copy.setTemplateValues(values);CHECK(image(*copy.asset())==before);
}
void failures(const std::filesystem::path &base,const std::filesystem::path &directory)
{
    Object3DEngine engine;CharacterEditor editor(fixture(engine));const auto count=engine.objectCount();const auto revision=editor.revision();
    auto missing=selection(directory,"does-not-exist");rejects([&]{(void)setHumanSkin(editor,missing);});
    auto small=selection(directory,"young_african_male");small.maximumBytes=1024;rejects([&]{(void)setHumanSkin(editor,small);});
    auto badMesh=selection(directory,"young_african_male");badMesh.mesh=99;rejects([&]{(void)setHumanSkin(editor,badMesh);});
    CHECK(editor.revision()==revision&&engine.objectCount()==count);
    CharacterEditor noUv(fixture(engine,false));const auto before=noUv.revision();
    rejects([&]{(void)setHumanSkin(noUv,selection(directory,"young_african_male"));});CHECK(noUv.revision()==before);
    MakeHumanTemplateOptions options;const auto assets=engine.objectCount(),templates=engine.reference().templateCount();
    rejects([&]{(void)loadMakeHumanTemplate(engine.reference(),base,options,missing);});
    bool preflight=false;
    try{(void)loadMakeHumanTemplate(engine.reference(),base/"missing-base",options,missing);}
    catch(const std::invalid_argument &e){preflight=std::string_view(e.what()).find("Unknown human skin preset")!=std::string_view::npos;}
    CHECK(preflight);
    CHECK(engine.objectCount()==assets&&engine.reference().templateCount()==templates);
    struct Scratch final
    {
        std::filesystem::path path;bool owned=false;
        ~Scratch(){if(owned){std::error_code ignored;std::filesystem::remove_all(path,ignored);}}
    } scratch{std::filesystem::current_path()/("human-skin-failures-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
    scratch.owned=std::filesystem::create_directory(scratch.path);CHECK(scratch.owned);
    const auto write=[&](std::string_view name,std::string_view value)
    {std::ofstream file(scratch.path/name,std::ios::binary);CHECK(file&&file.write(value.data(),static_cast<std::streamsize>(value.size())));};
    const auto setCatalog=[&](std::string_view rows)
    {write("catalog.json",std::string("{\"format\":\"iiActorObject.MakeHumanSkin.1\",\"baseMesh\":\"hm08\",\"presets\":[")+std::string(rows)+"]}");};
    const auto row=R"({"id":"test","label":"Test","texture":"bad.png","roughness":0.5})";
    setCatalog(std::string(row)+","+row);rejects([&]{(void)listHumanSkinPresets(scratch.path);});
    setCatalog(R"({"id":"test","label":"Test","texture":"../outside.png","roughness":0.5})");
    rejects([&]{(void)listHumanSkinPresets(scratch.path);});
    setCatalog(row);write("bad.png","not a png");auto corrupt=selection(scratch.path,"test");
    rejects([&]{(void)setHumanSkin(editor,corrupt);});CHECK(editor.revision()==revision);
    std::error_code ec;std::filesystem::create_directory_symlink(directory/"skins/young_african_male",scratch.path/"outside",ec);
    if(!ec)
    {
        setCatalog(R"({"id":"test","label":"Test","texture":"outside/young_darkskinned_male_diffuse.png","roughness":0.5})");
        rejects([&]{(void)setHumanSkin(editor,corrupt);});CHECK(editor.revision()==revision);
    }
    auto shared=fixture(engine)->data();shared.meshes.push_back(shared.meshes[0]);shared.meshes.back().name="Other";
    CharacterEditor sharedEditor(std::make_shared<CharacterAsset>(engine.createObject(std::move(shared))));
    const auto sharedRevision=sharedEditor.revision();rejects([&]{(void)setHumanSkin(sharedEditor,selection(directory,"young_african_male"));});
    CHECK(sharedEditor.revision()==sharedRevision);
    auto conflict=fixture(engine)->data();const auto png=base.parent_path()/"fixtures/images/blue.png";
    std::ifstream source(png,std::ios::binary);CHECK(source);std::vector<std::uint8_t> original((std::istreambuf_iterator<char>(source)),{});
    conflict.images.emplace_back("iiActorObject.HumanSkin.young_african_male",original);conflict.materials[0].baseColorImage=0;
    CharacterEditor collision(std::make_shared<CharacterAsset>(engine.createObject(std::move(conflict))));
    CHECK(setHumanSkin(collision,selection(directory,"young_african_male")));const auto collisionRevision=collision.revision();
    CHECK(collision.asset()->data().images.size()==2&&collision.asset()->data().images[0].encoded().size()==original.size());
    CHECK(std::equal(original.begin(),original.end(),collision.asset()->data().images[0].encoded().begin()));
    CHECK(!setHumanSkin(collision,selection(directory,"young_african_male"))&&collision.revision()==collisionRevision);
}
void formats(const std::filesystem::path &directory,const std::filesystem::path &output)
{
    Object3DEngine engine;CharacterEditor editor(fixture(engine));CHECK(setHumanSkin(editor,selection(directory,"young_african_male")));
    const auto write=[&](std::string_view name,const std::vector<std::uint8_t> &bytes)
    {
        std::ofstream file(output/name,std::ios::binary);if(!file||!file.write(reinterpret_cast<const char *>(bytes.data()),static_cast<std::streamsize>(bytes.size())))
            throw std::runtime_error("Cannot write format fixture");
    };
    write("skin.glb",exportObject3DGlb(editor.asset()->object().get()));write("skin.fbx",exportObject3DFbx(editor.asset()->object().get()));
}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=4&&argc!=5)throw std::invalid_argument("expected case and two asset directories");const std::string_view which=argv[1];
        if(which=="catalog")catalog(argv[3]);else if(which=="editing")editing(argv[3]);
        else if(which=="template")templateSkin(argv[2],argv[3]);else if(which=="failures")failures(argv[2],argv[3]);
        else if(which=="formats"&&argc==5)formats(argv[3],argv[4]);
        else throw std::invalid_argument("unknown case");
    }
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}return 0;
}
