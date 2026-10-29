#include "../src/Interchange/GLB/Object3DGlbMaterialWrite.h"
#include "../src/Interchange/GLB/Object3DGlbBuffer.h"
#include <iiActorObject/Object3DImage.h>
#include <iiActorObject/Object3DMaterial.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail::glb_export;
namespace
{
void check(bool value){if(!value)throw std::runtime_error("GLB material writer contract failed");}
template<class F> void rejects(F f){try{f();}catch(const std::exception &){return;}throw std::runtime_error("Invalid GLB material writer input accepted");}
}
int main(int argc,char **argv)
{
    check(argc==2);std::ifstream in(argv[1],std::ios::binary);check(bool(in));
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in),{}};
    std::vector<Object3DImage> images;images.emplace_back("Check\"er",bytes,"image/png");
    Object3DMaterial mapped;mapped.name="Paint";mapped.baseColorImage=0;mapped.normalImage=0;
    mapped.normalScale=.25;mapped.surface={Object3DAlphaMode::Mask,.3,true};
    Object3DMaterial plain;plain.name="Plain";plain.surface.alphaMode=Object3DAlphaMode::Opaque;
    std::vector<Object3DMaterial> materials{mapped,plain};
    const std::array<GlbMaterialVariant,2> variants{{{0,2,1},{1,0,0}}};
    Buffer buffer;const auto views=writeGlbImages(buffer,images);
    check(views==std::vector<std::size_t>({0})&&buffer.bytes==bytes&&buffer.views.size()==1);
    auto twoImages=images;twoImages.push_back(images[0]);Buffer twoBuffers;
    check(writeGlbImages(twoBuffers,twoImages)==std::vector<std::size_t>({0,1}));
    const auto secondOffset=(bytes.size()+3)&~std::size_t{3};
    check(twoBuffers.bytes.size()==secondOffset+bytes.size());
    check(std::equal(bytes.begin(),bytes.end(),twoBuffers.bytes.begin()+secondOffset));
    auto json=stream();json<<"{";writeGlbMaterials(json,materials,variants,images,views,true);json<<"}";
    check(json.str()==R"({,"materials":[{"name":"Paint","pbrMetallicRoughness":{"baseColorFactor":[0.55000000000000004,0.32000000000000001,0.22,1],"metallicFactor":0,"roughnessFactor":0.65000000000000002,"baseColorTexture":{"index":0,"texCoord":2}},"normalTexture":{"index":0,"texCoord":1,"scale":0.25},"alphaMode":"MASK","alphaCutoff":0.29999999999999999,"doubleSided":true,"extras":{"iiActorObjectSourceMaterial":0}},{"name":"Plain","pbrMetallicRoughness":{"baseColorFactor":[0.55000000000000004,0.32000000000000001,0.22,1],"metallicFactor":0,"roughnessFactor":0.65000000000000002},"alphaMode":"OPAQUE","extras":{"iiActorObjectSourceMaterial":1}}],"images":[{"name":"Check\"er","mimeType":"image/png","bufferView":0}],"samplers":[{"magFilter":9729,"minFilter":9729,"wrapS":10497,"wrapT":10497}],"textures":[{"source":0,"sampler":0}]})");
    auto noIdentity=stream();writeGlbMaterials(noIdentity,materials,variants,images,views,false);
    check(noIdentity.str().find("iiActorObjectSourceMaterial")==std::string::npos);
    auto automatic=materials;automatic[0].surface={};
    auto blend=stream();writeGlbMaterials(blend,automatic,variants,images,views,false);
    check(blend.str().find(R"("alphaMode":"BLEND")")!=std::string::npos);
    auto empty=stream();writeGlbMaterials(empty,{}, {}, {}, {},false);check(empty.str()==R"(,"materials":[])");
    auto unchanged=stream();unchanged<<"prefix";
    rejects([&]{writeGlbMaterials(unchanged,materials,variants,images,{},false);});
    const std::array<GlbMaterialVariant,1> invalid{{{2,0,0}}};
    rejects([&]{writeGlbMaterials(unchanged,materials,invalid,images,views,false);});
    check(unchanged.str()=="prefix");
}
