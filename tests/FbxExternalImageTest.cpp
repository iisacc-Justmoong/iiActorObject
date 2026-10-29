#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DTemplate.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace iiActorObject;
#define CHECK(x) do { if(!(x)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x); } while(false)
template<class F> void rejects(F f) { bool caught=false;try{f();}catch(const std::exception &){caught=true;}CHECK(caught); }
std::vector<std::uint8_t> read(const std::filesystem::path &path)
{std::ifstream file(path,std::ios::binary);CHECK(file.good());return {(std::istreambuf_iterator<char>(file)),{}};}
struct Resolver final:FbxImageResolver
{
    std::map<std::string,std::vector<std::uint8_t>> images;
    mutable std::vector<std::string> requests;
    mutable std::vector<std::size_t> budgets;
    bool badMime=false,fail=false;
    std::string notice;
    std::optional<FbxResolvedImage> resolve(const FbxImageRequest &request) const override
    {
        if(fail)throw std::runtime_error("resolver failed");
        requests.emplace_back(request.relativeFilename);budgets.push_back(request.remainingBytes);
        const auto found=images.find(std::string(request.relativeFilename));
        if(found==images.end())return std::nullopt;
        return FbxResolvedImage{found->second,badMime?"image/jpeg":"image/png",notice};
    }
};
bool contains(std::span<const std::uint8_t> a,std::span<const std::uint8_t> b)
{return a.size()==b.size()&&std::equal(a.begin(),a.end(),b.begin());}
void resolution(const std::vector<std::uint8_t> &fbx,const std::vector<std::uint8_t> &color,const std::vector<std::uint8_t> &normal)
{
    Object3DEngine engine;const auto unresolved=importObject3DFbx(engine.reference(),fbx);
    CHECK(unresolved.object.get().data().images.empty());CHECK(!unresolved.warnings.empty());
    Resolver resolver;resolver.images={{"textures/skin.png",color},{"textures/normal.png",normal}};
    FbxImportOptions options;options.imageResolver=&resolver;
    const auto imported=importObject3DFbx(engine.reference(),fbx,options);const auto &data=imported.object.get().data();
    CHECK(resolver.requests.size()==2&&data.images.size()==2&&data.materials.size()==1);
    CHECK(resolver.budgets[0]==options.maxImageBytes&&resolver.budgets[1]<resolver.budgets[0]);
    CHECK(data.materials[0].baseColorImage&&data.materials[0].normalImage);
    CHECK(contains(data.images[*data.materials[0].baseColorImage].encoded(),color));
    CHECK(contains(data.images[*data.materials[0].normalImage].encoded(),normal));
    CHECK(data.materials[0].normalUvSet=="SurfaceUV");
    auto copy=importObject3DFbx(engine.reference(),exportObject3DFbx(imported.object.get()));
    CHECK(copy.object.get().data().images.size()==2&&copy.object.get().data().materials[0].normalImage);
    Object3DTemplateDefinition definition;definition.base=imported.object;auto editor=engine.editTemplate(engine.createTemplate(definition));
    auto saved=exportObject3DProject(editor);Object3DEngine destination;auto restored=importObject3DProject(destination.reference(),saved);
    CHECK(restored.editor.get().object().get().data().images.size()==2);
    CHECK(contains(restored.editor.get().object().get().data().images[*data.materials[0].baseColorImage].encoded(),color));
}
void limits(const std::vector<std::uint8_t> &fbx,const std::vector<std::uint8_t> &color,const std::vector<std::uint8_t> &normal)
{
    Object3DEngine engine;auto guard=engine.createObject(parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"));
    Resolver resolver;resolver.images={{"textures/skin.png",color},{"textures/normal.png",normal}};
    FbxImportOptions options;options.imageResolver=&resolver;options.maxImageBytes=color.size()+normal.size()-1;
    rejects([&]{(void)importObject3DFbx(engine.reference(),fbx,options);});CHECK(guard.valid()&&engine.objectCount()==1);
    options.maxImageBytes=64*1024*1024;options.maxImageResolverRequests=1;
    rejects([&]{(void)importObject3DFbx(engine.reference(),fbx,options);});CHECK(engine.objectCount()==1);
    options.maxImageResolverRequests=4096;resolver.badMime=true;
    rejects([&]{(void)importObject3DFbx(engine.reference(),fbx,options);});CHECK(engine.objectCount()==1);
    resolver.badMime=false;resolver.notice=std::string(1025,'x');
    rejects([&]{(void)importObject3DFbx(engine.reference(),fbx,options);});CHECK(engine.objectCount()==1);
    resolver.notice.clear();options.maxImageResolverRequests=4097;
    rejects([&]{(void)importObject3DFbx(engine.reference(),fbx,options);});CHECK(engine.objectCount()==1);
    options.maxImageResolverRequests=4096;
    resolver.fail=true;
    rejects([&]{(void)importObject3DFbx(engine.reference(),fbx,options);});CHECK(engine.objectCount()==1);
    resolver.fail=false;resolver.images["textures/skin.png"].back()^=1;
    rejects([&]{(void)importObject3DFbx(engine.reference(),fbx,options);});CHECK(engine.objectCount()==1);
}
void missing(const std::vector<std::uint8_t> &fbx,const std::vector<std::uint8_t> &color)
{
    Object3DEngine engine;Resolver resolver;resolver.images={{"textures/skin.png",color}};
    FbxImportOptions options;options.imageResolver=&resolver;
    const auto imported=importObject3DFbx(engine.reference(),fbx,options);
    CHECK(imported.object.get().data().images.size()==1&&!imported.warnings.empty());
    CHECK(imported.object.get().data().materials[0].baseColorImage&&!imported.object.get().data().materials[0].normalImage);
    std::string unsafe(fbx.begin(),fbx.end());const std::string from="textures/normal.png",to="../../outside.png";
    for(std::size_t pos=0;(pos=unsafe.find(from,pos))!=std::string::npos;pos+=to.size())unsafe.replace(pos,from.size(),to);
    resolver.requests.clear();const std::vector<std::uint8_t> bytes(unsafe.begin(),unsafe.end());
    const auto observed=importObject3DFbx(engine.reference(),bytes,options);
    CHECK(!observed.warnings.empty()&&std::find(resolver.requests.begin(),resolver.requests.end(),to)!=resolver.requests.end());
}
void embedded(const std::vector<std::uint8_t> &fbx)
{
    Object3DEngine engine;Resolver resolver;resolver.fail=true;FbxImportOptions options;options.imageResolver=&resolver;
    const auto imported=importObject3DFbx(engine.reference(),fbx,options);
    CHECK(imported.object.get().data().images.size()==1&&resolver.requests.empty());
}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=6)return 2;const auto fbx=read(argv[2]),color=read(argv[3]),normal=read(argv[4]);const std::string_view mode=argv[1];
        if(mode=="resolution")resolution(fbx,color,normal);else if(mode=="limits")limits(fbx,color,normal);
        else if(mode=="missing")missing(fbx,color);else if(mode=="embedded")embedded(read(argv[5]));else return 2;
        std::cout<<mode<<" passed\n";return 0;
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
