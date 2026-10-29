#include "../src/Geometry/Object3DTemplateSurfaceReplacementLayers.h"
#include "../src/Core/Object3DAdjustmentData.h"
#include "../src/Geometry/Object3DTemplateSurfaceReplacementResources.h"
#include <fstream>
#include <iterator>
#include <memory>
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
{if(!value)throw std::runtime_error("Surface replacement layer test failed at line "+std::to_string(where.line()));}
template<class F>void rejects(F action,std::string_view expected)
{
    try{action();}
    catch(const std::exception &error)
    {if(expected!=error.what())throw std::runtime_error("Expected: "+std::string(expected)+"; received: "+error.what());return;}
    throw std::runtime_error("Invalid replacement layer accepted");
}
std::vector<std::uint8_t> bytes(const char *path)
{
    std::ifstream input(path,std::ios::binary);
    if(!input)throw std::runtime_error("Missing image fixture");
    return {std::istreambuf_iterator<char>(input),{}};
}
}
int main(int argc,char **argv)
{
    if(argc!=3)return 2;
    Object3DData before,after;
    before.images.emplace_back("Old",bytes(argv[1]));after.images.emplace_back("New",bytes(argv[2]));
    Object3DMaterial body,old,newMaterial;body.name="Body";old.name="Old";newMaterial.name="New";
    old.baseColorImage=0;newMaterial.baseColorImage=0;
    before.materials={body,old};after.materials={body,newMaterial};
    Object3DMesh mesh;mesh.material=0;before.meshes.push_back(mesh);after.meshes.push_back(mesh);
    mesh.material=1;before.meshes.push_back(mesh);after.meshes.push_back(mesh);
    TemplateSurfaceReplacementResources plan(before,after);plan.add(1);

    auto unchanged=std::make_shared<Adjustment>();
    const auto same=prepareTemplateSurfaceReplacementLayer(unchanged,plan,before);
    check(same.get()==unchanged.get());
    const std::shared_ptr<const Adjustment> missing;
    rejects([&]{(void)prepareTemplateSurfaceReplacementLayer(missing,plan,before);},
        "Replacement edit layer is missing");

    auto snapshot=std::make_shared<Adjustment>();snapshot->materials=before.materials;snapshot->images=before.images;
    snapshot->materials->at(0).surface.doubleSided=true;
    const auto updated=prepareTemplateSurfaceReplacementLayer(snapshot,plan,before);
    check(updated.get()!=snapshot.get()&&updated->materials->at(0).surface.doubleSided);
    check(updated->materials->at(1).name=="New"&&updated->materials->at(1).baseColorImage==0&&
        updated->images->at(0)==after.images[0]);
    check(snapshot->materials->at(1).name=="Old"&&snapshot->images->at(0)==before.images[0]);
    auto materialOnly=std::make_shared<Adjustment>(*snapshot);materialOnly->images.reset();
    const auto updatedMaterial=prepareTemplateSurfaceReplacementLayer(materialOnly,plan,before);
    check(updatedMaterial->materials->at(1).name=="New"&&!updatedMaterial->images);
    auto imageOnly=std::make_shared<Adjustment>(*snapshot);imageOnly->materials.reset();
    const auto updatedImage=prepareTemplateSurfaceReplacementLayer(imageOnly,plan,before);
    check(!updatedImage->materials&&updatedImage->images->at(0)==after.images[0]);

    auto assigned=std::make_shared<Adjustment>();MeshAdjustment assignedMesh;assignedMesh.material=1;
    assigned->meshes.push_back(assignedMesh);
    rejects([&]{(void)prepareTemplateSurfaceReplacementLayer(assigned,plan,before);},
        "Replacement material is assigned by an edit layer");
    assigned->meshes[0].material.reset();assigned->meshes[0].triangleMaterials=std::vector<std::size_t>{0,1};
    rejects([&]{(void)prepareTemplateSurfaceReplacementLayer(assigned,plan,before);},
        "Replacement material is assigned by an edit layer");
    auto changedMaterial=std::make_shared<Adjustment>(*snapshot);changedMaterial->materials->at(1).name="Edited";
    rejects([&]{(void)prepareTemplateSurfaceReplacementLayer(changedMaterial,plan,before);},
        "Replacement material has an edit layer");
    auto shortMaterials=std::make_shared<Adjustment>(*snapshot);shortMaterials->materials->resize(1);
    rejects([&]{(void)prepareTemplateSurfaceReplacementLayer(shortMaterials,plan,before);},
        "Replacement material has an edit layer");
    auto sharedImage=std::make_shared<Adjustment>(*snapshot);sharedImage->materials->at(0).baseColorImage=0;
    rejects([&]{(void)prepareTemplateSurfaceReplacementLayer(sharedImage,plan,before);},
        "Replacement image is shared by an edit layer");
    auto changedImage=std::make_shared<Adjustment>(*snapshot);changedImage->images->at(0)=after.images[0];
    rejects([&]{(void)prepareTemplateSurfaceReplacementLayer(changedImage,plan,before);},
        "Replacement image has an edit layer");
    auto shortImages=std::make_shared<Adjustment>(*snapshot);shortImages->images->clear();
    rejects([&]{(void)prepareTemplateSurfaceReplacementLayer(shortImages,plan,before);},
        "Replacement image has an edit layer");
    check(snapshot->materials->at(1).name=="Old"&&snapshot->images->at(0)==before.images[0]);

    TemplateSurfaceReplacementResources empty(before,after);
    const auto kept=prepareTemplateSurfaceReplacementLayer(snapshot,empty,before);
    check(kept.get()==snapshot.get());
}
