#include "../src/Geometry/Object3DTemplateSurfaceReplacementResources.h"
#include <fstream>
#include <iterator>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool value,const std::source_location &where=std::source_location::current())
{if(!value)throw std::runtime_error("Surface replacement resources test failed at line "+std::to_string(where.line()));}
template<class F>void rejects(F action,std::string_view expected)
{
    try{action();}
    catch(const std::exception &error)
    {if(expected!=error.what())throw std::runtime_error("Expected: "+std::string(expected)+"; received: "+error.what());return;}
    throw std::runtime_error("Invalid replacement resources accepted");
}
std::vector<std::uint8_t> bytes(const char *path)
{
    std::ifstream input(path,std::ios::binary);
    if(!input)throw std::runtime_error("Missing image fixture");
    return {std::istreambuf_iterator<char>(input),{}};
}
struct Examples final
{
    Object3DData before,after;
    Examples(const char *first,const char *second)
    {
        const auto a=bytes(first),b=bytes(second);
        before.images={{"Old color",a},{"Old normal",b},{"Old hat color",b},{"Old hat normal",a}};
        after.images={{"New hat color",a},{"New hat normal",b},{"New color",b},{"New normal",a}};
        Object3DMaterial body,coat,hat;body.name="Body";coat.name="Old coat";hat.name="Old hat";
        coat.baseColorImage=0;coat.normalImage=1;hat.baseColorImage=2;hat.normalImage=3;
        before.materials={body,coat,hat};
        coat.name="New coat";coat.baseColorImage=2;coat.normalImage=3;
        hat.name="New hat";hat.baseColorImage=0;hat.normalImage=1;
        after.materials={body,coat,hat};
        Object3DMesh mesh;mesh.material=0;before.meshes.push_back(mesh);after.meshes.push_back(mesh);
        mesh.material=1;before.meshes.push_back(mesh);after.meshes.push_back(mesh);
        mesh.material=2;before.meshes.push_back(mesh);after.meshes.push_back(mesh);
    }
};
}
int main(int argc,char **argv)
{
    if(argc!=3)return 2;
    Examples sample(argv[1],argv[2]);
    TemplateSurfaceReplacementResources plan(sample.before,sample.after);
    plan.add(1);plan.add(2);
    check(plan.materials().size()==2&&plan.images().size()==4);
    check(plan.oldMaterials().contains(1)&&plan.oldMaterials().contains(2)&&!plan.oldMaterials().contains(0));
    check(plan.materials()[0].index==1&&plan.materials()[0].replacement.name=="New coat"&&
        plan.materials()[0].replacement.baseColorImage==0&&plan.materials()[0].replacement.normalImage==1);
    check(plan.materials()[1].index==2&&plan.materials()[1].replacement.name=="New hat"&&
        plan.materials()[1].replacement.baseColorImage==2&&plan.materials()[1].replacement.normalImage==3);
    check(plan.images()[0].index==0&&plan.images()[0].replacement==sample.after.images[2]&&
        plan.images()[3].index==3&&plan.images()[3].replacement==sample.after.images[1]);
    check(sample.before.materials[1].name=="Old coat"&&sample.after.materials[1].baseColorImage==2);
    rejects([&]{plan.add(1);},"Replacement surfaces share a material");

    Examples multiple(argv[1],argv[2]);multiple.before.meshes[1].triangleMaterials={1,2};
    rejects([&]{TemplateSurfaceReplacementResources invalid(multiple.before,multiple.after);invalid.add(1);},
        "Template surface replacement requires one material per replaced mesh");
    Examples sharedMesh(argv[1],argv[2]);sharedMesh.before.meshes[0].material=1;
    rejects([&]{TemplateSurfaceReplacementResources invalid(sharedMesh.before,sharedMesh.after);invalid.add(1);},
        "Replacement material is shared");
    Examples sharedCandidate(argv[1],argv[2]);sharedCandidate.after.meshes[0].material=1;
    rejects([&]{TemplateSurfaceReplacementResources invalid(sharedCandidate.before,sharedCandidate.after);invalid.add(1);},
        "Replacement candidate material is shared");
    Examples count(argv[1],argv[2]);count.after.materials[1].normalImage.reset();
    rejects([&]{TemplateSurfaceReplacementResources invalid(count.before,count.after);invalid.add(1);},
        "Replacement image slot count changed");
    Examples duplicate(argv[1],argv[2]);duplicate.before.materials[1].normalImage=0;
    rejects([&]{TemplateSurfaceReplacementResources invalid(duplicate.before,duplicate.after);invalid.add(1);},
        "Replacement image is shared");
    Examples sharedOldImage(argv[1],argv[2]);sharedOldImage.before.materials[0].baseColorImage=0;
    rejects([&]{TemplateSurfaceReplacementResources invalid(sharedOldImage.before,sharedOldImage.after);invalid.add(1);},
        "Replacement image is shared by another material");
    Examples sharedNewImage(argv[1],argv[2]);sharedNewImage.after.materials[0].baseColorImage=2;
    rejects([&]{TemplateSurfaceReplacementResources invalid(sharedNewImage.before,sharedNewImage.after);invalid.add(1);},
        "Replacement candidate image is shared by another material");
    Examples untextured(argv[1],argv[2]);
    untextured.before.materials[1].baseColorImage.reset();untextured.before.materials[1].normalImage.reset();
    untextured.after.materials[1].baseColorImage.reset();untextured.after.materials[1].normalImage.reset();
    TemplateSurfaceReplacementResources plain(untextured.before,untextured.after);plain.add(1);
    check(plain.images().empty()&&plain.materials().size()==1&&!plain.materials()[0].replacement.baseColorImage);
}
