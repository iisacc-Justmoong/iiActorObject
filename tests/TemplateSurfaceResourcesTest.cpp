#include "../src/Geometry/Object3DTemplateSurfaceResources.h"
#include <iiActorObject/Object3DData.h>
#include <array>
#include <fstream>
#include <iterator>
#include <stdexcept>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool value)
{if(!value)throw std::runtime_error("Template surface resources contract failed");}
template<class F>void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid template resources were accepted");}
std::vector<std::uint8_t> bytes(const char *path)
{std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Missing image fixture");return {std::istreambuf_iterator<char>(input),{}};}
}
int main(int argc,char **argv)
{
    if(argc!=3)return 2;
    Object3DData base,candidate;
    base.images.emplace_back("Base",bytes(argv[1]));
    Object3DMaterial skin;skin.name="Skin";skin.baseColorImage=0;base.materials={skin};
    candidate.images=base.images;candidate.images.emplace_back("Base",bytes(argv[2]));
    auto cloth=skin;cloth.baseColorImage=1;candidate.materials={skin,cloth};
    auto layerImages=base.images;auto layerMaterials=base.materials;
    const std::array layers{TemplateSurfaceResourceLayer{&layerImages,&layerMaterials}};
    TemplateSurfaceResources resources(base,candidate,layers);
    resources.linkMaterial(0,0);resources.linkRetainedImages();
    check(resources.material(0)==0&&resources.material(1)==1&&resources.material(1)==1);
    check(base.images.size()==2&&layerImages.size()==2&&base.materials.size()==2&&layerMaterials.size()==2);
    check(base.materials[1].baseColorImage==1&&sameTemplateMaterial(base.materials[1],layerMaterials[1]));
    check(base.images[1].name()=="iiActorObject.AddedImage.1"&&base.materials[1].name=="iiActorObject.AddedMaterial.1");
    check(candidate.images[1].name()=="Base"&&candidate.materials[1].name=="Skin");
    rejects([&]{resources.linkMaterial(0,1);});

    Object3DMesh mesh;mesh.material=1;mesh.triangleMaterials={1,1};
    check(templateMeshUsesMaterial(mesh,1)&&!templateMeshUsesMaterial(mesh,0));
    check(templateMaterialUsesImage(base.materials[1],1));requireSingleTemplateMaterial(mesh);
    mesh.triangleMaterials.push_back(0);rejects([&]{requireSingleTemplateMaterial(mesh);});
    mesh.topology.emplace();rejects([&]{requireSingleTemplateMaterial(mesh);});
    rejects([&]{(void)templateMeshUsesMaterial(mesh,0);});

    Object3DData paddedBase,paddedCandidate;
    paddedBase.images=base.images;paddedBase.images.resize(1);paddedBase.materials={skin};
    paddedCandidate.images=base.images;paddedCandidate.materials=base.materials;
    auto longImages=base.images;auto longMaterials=base.materials;
    auto shortImages=paddedBase.images;auto shortMaterials=paddedBase.materials;
    const std::array paddingLayers{TemplateSurfaceResourceLayer{&longImages,&longMaterials},
                                   TemplateSurfaceResourceLayer{&shortImages,&shortMaterials}};
    TemplateSurfaceResources padded(paddedBase,paddedCandidate,paddingLayers);
    check(paddedBase.images.size()==2&&shortImages.size()==2&&longImages.size()==2);
    check(paddedBase.images[1]==shortImages[1]&&paddedBase.materials[1].name==shortMaterials[1].name);
    check(paddedBase.images[1].name()=="iiActorObject.PaddingImage.1");
    check(padded.material(0)==0&&padded.material(1)==2);
    check(paddedBase.images.size()==3&&longImages.size()==3&&shortImages.size()==3);
    check(paddedBase.materials.size()==3&&longMaterials.size()==3&&shortMaterials.size()==3);

    Object3DData old,combined;old.images.emplace_back("A",bytes(argv[1]));old.images.emplace_back("B",bytes(argv[2]));
    auto a=skin;a.name="A";a.baseColorImage=0;auto b=skin;b.name="B";b.baseColorImage=1;old.materials={a,b};
    combined.images={old.images[0]};auto c=a;c.name="C";combined.materials={a,a,c};
    TemplateSurfaceResources ambiguous(old,combined,{});ambiguous.linkMaterial(0,0);ambiguous.linkMaterial(1,1);
    ambiguous.linkRetainedImages();rejects([&]{(void)ambiguous.material(2);});
}
