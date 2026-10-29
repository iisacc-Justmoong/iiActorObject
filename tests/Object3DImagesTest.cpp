#include <iiActorObject/Object3DImage.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
std::vector<std::uint8_t> read(const std::filesystem::path &p){std::ifstream file(p,std::ios::binary);CHECK(file.good());return {(std::istreambuf_iterator<char>(file)),{}};}
void save(const std::filesystem::path &p,const std::vector<std::uint8_t> &bytes){std::ofstream file(p,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(file.good());}
Object3DData data(const std::vector<std::uint8_t> &png)
{
    auto d=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nf 1/1 2/2 3/3 4/4\n");
    d.images.emplace_back("Checker",png);d.materials[0].baseColor={1,1,1,1};d.materials[0].baseColorImage=0;return d;
}
void immutable(const std::vector<std::uint8_t> &png)
{
    Object3DImage image("Checker",png);auto copy=image;CHECK(image.width()==8&&image.height()==8&&image.mimeType()=="image/png"&&copy==image);CHECK(copy.encoded().data()==image.encoded().data());
    auto other=Object3DImage("Checker",png);CHECK(image==other&&image.encoded().data()!=other.encoded().data());Object3DImage empty;CHECK(empty.empty());
    rejects([&]{(void)Object3DImage("Checker",png,"image/jpeg");});auto malformed=png;malformed[malformed.size()-1]^=1;rejects([&]{(void)Object3DImage("Checker",malformed);});
}
void validation(const std::vector<std::uint8_t> &png)
{
    Object3DEngine engine;auto d=data(png);auto valid=engine.createObject(d);const auto count=engine.objectCount();d.materials[0].baseColorImage=1;rejects([&]{(void)engine.createObject(d);});CHECK(engine.objectCount()==count);
    d=data(png);d.images.push_back(d.images[0]);rejects([&]{(void)engine.createObject(d);});d=data(png);d.images[0]={};rejects([&]{(void)engine.createObject(d);});
}
void formats(const std::vector<std::uint8_t> &png,const std::filesystem::path &output)
{
    Object3DEngine engine;auto object=engine.createObject(data(png));auto fbx=exportObject3DFbx(object.get());auto imported=importObject3DFbx(engine.reference(),fbx);CHECK(imported.object.get().data().images.size()==1&&imported.object.get().data().materials[0].baseColorImage==0);CHECK(imported.object.get().data().images[0]==object.get().data().images[0]);
    std::filesystem::create_directories(output);save(output/"Checker.glb",exportObject3DGlb(object.get()));save(output/"Checker.fbx",fbx);
}
void history(const std::vector<std::uint8_t> &png)
{
    Object3DEngine engine;auto source=data(png);source.images.clear();source.materials[0].baseColorImage.reset();Object3DTemplateDefinition program;program.base=engine.createObject(source);program.targets={{"shape",0,{{1,{.1,0,0}}}}};auto e=engine.editTemplate(engine.createTemplate(program));auto replacement=e.get().object().get().data();replacement.images.emplace_back("Checker",png);replacement.materials[0].baseColorImage=0;e.get().replace(replacement);e.get().setTemplateValues(std::array{TemplateValue{"shape",.5}});CHECK(e.get().object().get().data().images.size()==1&&e.get().object().get().data().materials[0].baseColorImage==0);CHECK(e.get().undo()&&e.get().undo());CHECK(e.get().object().get().data().images.empty());CHECK(e.get().redo());
    auto bytes=exportObject3DProject(e);CHECK(bytes[8]==8);Object3DEngine restored;auto project=importObject3DProject(restored.reference(),bytes);CHECK(project.editor.get().object().get().data().images[0]==replacement.images[0]);CHECK(project.editor.get().undo()&&project.editor.get().redo());
}
int main(int argc,char **argv)
{
    try{if(argc!=4)return 2;const auto png=read(argv[2]);const std::string_view mode=argv[1];if(mode=="immutable")immutable(png);else if(mode=="validation")validation(png);else if(mode=="formats")formats(png,argv[3]);else if(mode=="history")history(png);else return 2;std::cout<<mode<<" passed\n";}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
