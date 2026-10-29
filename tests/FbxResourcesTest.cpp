#include "../src/Interchange/FBX/Object3DFbxResources.h"
#include "../src/Interchange/FBX/Object3DFbxImportFields.h"
#include <array>
#include <stdexcept>
#include <string>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool condition)
{if(!condition)throw std::runtime_error("FBX resource reader contract failed");}
template<class F> void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid FBX resource was accepted");}
std::vector<std::uint8_t> png()
{
    return {0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,
        0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x08,0x08,0x06,0x00,0x00,0x00,0xc4,0x0f,0xbe,0x8b,
        0x00,0x00,0x00,0x11,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0x60,0x60,0xf8,0xff,0x1f,0x3f,
        0x1e,0x11,0x0a,0x00,0x43,0x57,0x7f,0x81,0x9b,0x5b,0x75,0x3b,0x00,0x00,0x00,0x00,
        0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82};
}
struct Resolver final:FbxImageResolver
{
    mutable std::size_t calls=0,budget=0;
    std::optional<FbxResolvedImage> resolve(const FbxImageRequest &request) const override
    {
        ++calls;budget=request.remainingBytes;check(request.relativeFilename=="external.png");
        return FbxResolvedImage{png(),"image/png","converted"};
    }
};
}
int main()
{
    std::vector<std::string> warnings;
    const FbxWarningSink warn=[&](std::string text){warnings.push_back(std::move(text));};
    FbxTextureReadOptions options{1024,4,nullptr};
    ufbx_scene scene{};
    auto empty=readFbxTextures(scene,options,warn);
    auto defaultMaterials=readFbxMaterials(scene,empty,true,false,warn);
    check(empty.images.empty()&&defaultMaterials.materials.size()==1);

    const auto image=png();ufbx_video video{};ufbx_texture first{},second{};
    first.name={"color",5};first.uv_set={"SurfaceUV",9};first.video=&video;
    first.type=UFBX_TEXTURE_FILE;first.content={image.data(),image.size()};
    second=first;second.name={"normal",6};second.content={nullptr,0};
    std::array<ufbx_texture *,2> textures{&first,&second};scene.textures={textures.data(),textures.size()};
    auto loaded=readFbxTextures(scene,options,warn);
    check(loaded.images.size()==1&&loaded.images[0].name()=="color"&&loaded.images[0].width()==8);
    check(loaded.textureImages.at(&first)==0&&loaded.textureImages.at(&second)==0);
    first.name={nullptr,5};
    rejects([&]{(void)readFbxTextures(scene,options,warn);});
    first.name={"color",5};
    ufbx_prop invalidText{};invalidText.type=UFBX_PROP_STRING;invalidText.value_str={nullptr,3};
    rejects([&]{(void)fbx_fields::externalText(&invalidText);});
    ufbx_material material{};material.name={"Surface",7};
    material.pbr.base_color.has_value=true;material.pbr.base_color.value_vec3.x=.2;
    material.pbr.base_color.value_vec3.y=.4;material.pbr.base_color.value_vec3.z=.6;
    material.pbr.base_color.texture=&first;material.pbr.normal_map.texture=&second;
    material.pbr.normal_map.texture_enabled=true;
    material.pbr.roughness.has_value=true;material.pbr.roughness.value_real=.25;
    std::array<ufbx_material *,1> materials{&material};scene.materials={materials.data(),materials.size()};
    auto decoded=readFbxMaterials(scene,loaded,true,false,warn);
    check(decoded.materials.size()==1&&decoded.materials[0].name=="Surface");
    check(decoded.materials[0].baseColorImage==0&&decoded.materials[0].normalImage==0);
    check(decoded.materials[0].baseColorUvSet=="SurfaceUV"&&decoded.materials[0].normalUvSet=="SurfaceUV");
    check(decoded.materials[0].baseColor[0]==.2&&decoded.materials[0].roughness==.25);
    check(decoded.materialIndex.at(&material)==0);
    first.has_uv_transform=true;
    auto unsupported=readFbxMaterials(scene,loaded,true,false,warn);
    check(!unsupported.materials[0].baseColorImage&&!warnings.empty());
    first.has_uv_transform=false;

    first.video=nullptr;first.content={nullptr,0};first.relative_filename={"external.png",12};
    scene.textures={textures.data(),1};Resolver resolver;options.resolver=&resolver;
    auto resolved=readFbxTextures(scene,options,warn);
    check(resolver.calls==1&&resolver.budget==1024&&resolved.images.size()==1);
    options.maxImageBytes=image.size()-1;
    rejects([&]{(void)readFbxTextures(scene,options,warn);});
    options.maxImageBytes=1024;options.maxResolverRequests=0;
    rejects([&]{(void)readFbxTextures(scene,options,warn);});
}
