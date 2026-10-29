#include <iiActorObject/FbxVisualImageResolver.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiVisualCodec/ImageCodec.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace iiActorObject;
namespace image=ii::visual::image;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
std::vector<std::uint8_t> read(const std::filesystem::path &path)
{std::ifstream f(path,std::ios::binary);CHECK(f.good());return {(std::istreambuf_iterator<char>(f)),{}};}
struct MemorySource final:FbxExternalImageSource
{
    std::map<std::string,std::vector<std::uint8_t>> images;
    mutable std::size_t calls=0;
    bool fail=false;
    std::optional<std::vector<std::uint8_t>> read(const FbxImageRequest &request,std::size_t maximumBytes)const override
    {
        ++calls;if(fail)throw std::runtime_error("source must not be called");CHECK(maximumBytes>0);auto found=images.find(std::string(request.relativeFilename));
        return found==images.end()?std::nullopt:std::optional(found->second);
    }
};
std::vector<std::uint8_t> fixture(std::vector<std::uint8_t> input)
{
    std::string text(input.begin(),input.end());
    for(const auto [from,to]:{std::pair{"textures/skin.png","textures/skin.jpg"},std::pair{"textures/normal.png","textures/normal.tga"}})
        for(std::size_t at=0;(at=text.find(from,at))!=std::string::npos;at+=std::char_traits<char>::length(to))
            text.replace(at,std::char_traits<char>::length(from),to);
    return {text.begin(),text.end()};
}
struct Samples final
{
    std::vector<std::uint8_t> jpeg,tga;
    Samples()
    {
        const std::array<std::uint8_t,12> pixels{255,0,0,0,255,0,0,0,255,255,255,0};
        const image::ImageView view{2,2,image::PixelFormat::rgb8,0,pixels};
        auto a=image::encode_jpeg(view,95),b=image::encode(view,image::Format::tga);
        if(!a)throw std::runtime_error("JPEG fixture encode: "+a.error().message);
        if(!b)throw std::runtime_error("TGA fixture encode: "+b.error().message);
        jpeg=std::move(*a);tga=std::move(*b);
    }
};
void resolution(const std::vector<std::uint8_t> &fbx)
{
    Samples samples;MemorySource source;source.images={{"textures/skin.jpg",samples.jpeg},{"textures/normal.tga",samples.tga}};
    FbxVisualImageResolver resolver(source);FbxImportOptions options;options.imageResolver=&resolver;
    Object3DEngine engine;auto imported=importObject3DFbx(engine.reference(),fixture(fbx),options);const auto &data=imported.object.get().data();
    CHECK(source.calls==2&&data.images.size()==2&&data.materials[0].baseColorImage&&data.materials[0].normalImage);
    CHECK(std::any_of(imported.warnings.begin(),imported.warnings.end(),[](const auto &w){return w.find("transcoded")!=w.npos;}));
    const auto &color=data.images[*data.materials[0].baseColorImage],&normal=data.images[*data.materials[0].normalImage];
    CHECK(color.mimeType()=="image/png"&&normal.mimeType()=="image/png"&&color.width()==2&&color.height()==2);
    auto expectedColor=image::decode(samples.jpeg),actualColor=image::decode(color.encoded());
    auto expectedNormal=image::decode(samples.tga),actualNormal=image::decode(normal.encoded());
    CHECK(expectedColor&&actualColor&&expectedNormal&&actualNormal);
    CHECK(expectedColor->rgba==actualColor->rgba&&expectedNormal->rgba==actualNormal->rgba);
}
void limits(const std::vector<std::uint8_t> &fbx)
{
    Samples samples;MemorySource source;source.images={{"textures/skin.jpg",samples.jpeg},{"textures/normal.tga",samples.tga}};
    Object3DEngine engine;FbxVisualImageLimits limits;limits.maxSourceBytes=1;FbxVisualImageResolver small(source,limits);
    FbxImportOptions options;options.imageResolver=&small;
    rejects([&]{(void)importObject3DFbx(engine.reference(),fixture(fbx),options);});CHECK(engine.objectCount()==0);
    limits.maxSourceBytes=64*1024*1024;limits.maxDimension=1;FbxVisualImageResolver dimensions(source,limits);options.imageResolver=&dimensions;
    rejects([&]{(void)importObject3DFbx(engine.reference(),fixture(fbx),options);});CHECK(engine.objectCount()==0);
    limits.maxDimension=16384;FbxVisualImageResolver output(source,limits);options.imageResolver=&output;options.maxImageBytes=1;
    rejects([&]{(void)importObject3DFbx(engine.reference(),fixture(fbx),options);});CHECK(engine.objectCount()==0);
    options.maxImageBytes=64*1024*1024;source.images["textures/skin.jpg"]={0,1,2,3};
    rejects([&]{(void)importObject3DFbx(engine.reference(),fixture(fbx),options);});CHECK(engine.objectCount()==0);
}
void passthrough(const std::vector<std::uint8_t> &fbx,const std::vector<std::uint8_t> &png)
{
    MemorySource source;source.images={{"textures/skin.png",png}};FbxVisualImageResolver resolver(source);
    FbxImportOptions options;options.imageResolver=&resolver;Object3DEngine engine;
    auto imported=importObject3DFbx(engine.reference(),fbx,options);const auto &image=imported.object.get().data().images.at(0);
    CHECK(image.encoded().size()==png.size()&&std::equal(image.encoded().begin(),image.encoded().end(),png.begin()));
    CHECK(!imported.warnings.empty());
    FbxVisualImageLimits limits;limits.maxDimension=1;FbxVisualImageResolver tooSmall(source,limits);options.imageResolver=&tooSmall;
    const auto count=engine.objectCount();rejects([&]{(void)importObject3DFbx(engine.reference(),fbx,options);});CHECK(engine.objectCount()==count);
    limits.maxDimension=0;rejects([&]{(void)FbxVisualImageResolver(source,limits);});
}
std::string base64(std::span<const std::uint8_t> bytes)
{
    constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string result;
    for(std::size_t i=0;i<bytes.size();i+=3)
    {
        const std::uint32_t n=(std::uint32_t(bytes[i])<<16)|(i+1<bytes.size()?std::uint32_t(bytes[i+1])<<8:0)|(i+2<bytes.size()?bytes[i+2]:0);
        result.push_back(alphabet[(n>>18)&63]);result.push_back(alphabet[(n>>12)&63]);
        result.push_back(i+1<bytes.size()?alphabet[(n>>6)&63]:'=');result.push_back(i+2<bytes.size()?alphabet[n&63]:'=');
    }
    return result;
}
void embedded(const std::vector<std::uint8_t> &input)
{
    Samples samples;std::string fbx(input.begin(),input.end());const std::string marker="Content: , \"";
    const auto begin=fbx.find(marker);CHECK(begin!=fbx.npos);const auto start=begin+marker.size(),end=fbx.find('"',start);CHECK(end!=fbx.npos);
    fbx.replace(start,end-start,base64(samples.jpeg));MemorySource source;source.fail=true;FbxVisualImageResolver resolver(source);
    FbxImportOptions options;options.imageResolver=&resolver;Object3DEngine engine;
    const auto bytes=std::span(reinterpret_cast<const std::uint8_t*>(fbx.data()),fbx.size());
    auto without=importObject3DFbx(engine.reference(),bytes);CHECK(without.object.get().data().images.empty()&&!without.warnings.empty());
    auto imported=importObject3DFbx(engine.reference(),bytes,options);
    CHECK(source.calls==0&&imported.object.get().data().materials[0].normalImage);
    const auto &stored=imported.object.get().data().images[*imported.object.get().data().materials[0].normalImage];
    auto expected=image::decode(samples.jpeg),actual=image::decode(stored.encoded());CHECK(expected&&actual&&expected->rgba==actual->rgba);
    CHECK(std::any_of(imported.warnings.begin(),imported.warnings.end(),[](const auto &w){return w.find("transcoded")!=w.npos;}));
    options.maxImageBytes=samples.jpeg.size()-1;const auto count=engine.objectCount();
    rejects([&]{(void)importObject3DFbx(engine.reference(),bytes,options);});CHECK(engine.objectCount()==count);
}
void independent(const std::vector<std::uint8_t> &fbx, bool glb)
{
    Samples samples;MemorySource source;source.images={{"textures/skin.jpg",samples.jpeg},{"textures/normal.tga",samples.tga}};
    FbxVisualImageResolver resolver(source);FbxImportOptions options;options.imageResolver=&resolver;
    Object3DEngine engine;const auto imported=importObject3DFbx(engine.reference(),fixture(fbx),options);
    const auto bytes=glb?exportObject3DGlb(imported.object.get()):exportObject3DFbx(imported.object.get());
    for(const auto byte:bytes)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(byte);
    std::cout<<'\n';
}
int main(int argc,char **argv)
{
    try{if(argc!=5)return 2;const auto fbx=read(argv[2]);const std::string_view mode=argv[1];
        if(mode=="independent_glb"||mode=="independent_fbx"){independent(fbx,mode=="independent_glb");return 0;}
        if(mode=="resolution")resolution(fbx);else if(mode=="limits")limits(fbx);else if(mode=="passthrough")passthrough(fbx,read(argv[3]));else if(mode=="embedded")embedded(read(argv[4]));else return 2;
        std::cout<<mode<<" passed\n";return 0;}
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
