#include "../src/Project/Object3DProjectCodec.h"
#include "../src/Project/Object3DProjectArchive.h"
#include <iiActorObject/Object3DProjectOptions.h>
#include "../src/Project/Object3DProjectVersion.h"
#include <algorithm>
#include <array>
#include <stdexcept>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool value)
{if(!value)throw std::runtime_error("Project codec contract failed");}
template<class F>void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid project archive was accepted");}
ProjectArchive graph()
{
    ProjectArchive value;Object3DData asset;asset.name="Fixture";
    Object3DMesh mesh;mesh.name="Triangle";
    mesh.geometry.vertices={{{0,0,0},{0,0,1},{}},{{1,0,0},{0,0,1},{}},{{0,1,0},{0,0,1},{}}};
    mesh.geometry.triangles={0,1,2};asset.meshes.push_back(std::move(mesh));
    value.assets.push_back(std::move(asset));return value;
}
}
int main()
{
    Object3DProjectOptions options;
    auto source=graph();check(selectProjectVersion(source)==8);const auto bytes=encodeProjectArchive(source,options);
    const std::array<std::uint8_t,8> magic{'i','i','A','O','P','R','J',0};
    check(bytes.size()>24&&std::equal(magic.begin(),magic.end(),bytes.begin())&&bytes[8]==8);
    auto decoded=decodeProjectArchive(bytes,options);
    check(decoded.formatVersion==8&&decoded.assets.size()==1&&decoded.assets[0].name=="Fixture");
    check(decoded.assets[0].meshes[0].geometry.triangles==std::vector<std::uint32_t>({0,1,2}));
    assertEquivalentProjectAssets(source.assets[0],decoded.assets[0],options);
    decoded.assets[0].name="Changed";rejects([&]{assertEquivalentProjectAssets(source.assets[0],decoded.assets[0],options);});

    auto corrupt=bytes;corrupt.back()^=1;rejects([&]{(void)decodeProjectArchive(corrupt,options);});
    corrupt=bytes;corrupt[8]=0;rejects([&]{(void)decodeProjectArchive(corrupt,options);});
    corrupt=bytes;corrupt.pop_back();rejects([&]{(void)decodeProjectArchive(corrupt,options);});
    auto tight=options;tight.maximumBytes=24;rejects([&]{(void)encodeProjectArchive(source,tight);});
    tight=options;tight.maximumDecodedBytes=1;rejects([&]{(void)decodeProjectArchive(bytes,tight);});
    tight=options;tight.maximumAssets=0;rejects([&]{validateProjectLimits(tight);});
    ProjectArchive empty;rejects([&]{(void)encodeProjectArchive(empty,options);});

    source.assets[0].meshes[0].morphDefaults.push_back({"Smile",.25});
    const auto morph=encodeProjectArchive(source,options);check(morph[8]==23&&selectProjectVersion(source)==23);
    const auto morphDecoded=decodeProjectArchive(morph,options);
    check(morphDecoded.assets[0].meshes[0].morphDefaults[0].value==.25);
    source.progressiveMorphPrograms.push_back({});
    const auto progressive=encodeProjectArchive(source,options);check(progressive[8]==31&&selectProjectVersion(source)==31);
    check(decodeProjectArchive(progressive,options).progressiveMorphPrograms.size()==1);

    auto selections=graph();ProjectEntry before,after;after.selectionRemap={0,std::nullopt};
    selections.history={before,after};check(selectProjectVersion(selections)==33);
    selections.assets[0].meshes.push_back(selections.assets[0].meshes[0]);
    check(selectProjectVersion(selections)==35);
    selections.history[1].selectionRemap={0};check(selectProjectVersion(selections)==34);
}
