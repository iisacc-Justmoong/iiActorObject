#include "../src/Interchange/GLB/Object3DGlbSceneWrite.h"
#include "../src/Interchange/GLB/Object3DGlbBuffer.h"
#include <iiActorObject/Object3DData.h>
#include <stdexcept>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail::glb_export;
namespace
{
void check(bool value){if(!value)throw std::runtime_error("GLB scene writer contract failed");}
}
int main()
{
    Object3DData data;data.name="A\"ctor";data.meshes.resize(2);
    data.meshes[0].name="Body";data.meshes[1].name="Eyes";
    data.bones={{"Root",NoBone,{}},{"Face",0,{}}};data.semantics={{"face",1}};
    auto json=stream();writeGlbScene(json,data.name,data.bones,data.meshes,data.semantics,
        R"([{"id":1}])",true,7,"doc");
    const auto text=json.str();
    check(text.find(R"("iiActorObjectExternalDocument":"doc")")!=text.npos);
    check(text.find(R"("scenes":[{"nodes":[2]}])")!=text.npos);
    check(text.find(R"("name":"Root","translation":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1],"extras":{"iiActorObjectSourceBone":0},"children":[1])")!=text.npos);
    check(text.find(R"("name":"A\"ctor","children":[0,3,4],"extras":{"iiObject3D":{"schemaVersion":1,"semantics":{"face":1},"rigSamples":[{"id":1}]}}})")!=text.npos);
    check(text.find(R"("name":"Body","mesh":0,"skin":0},{"name":"Eyes","mesh":1,"skin":0})")!=text.npos);
    check(text.ends_with(R"(,"skins":[{"inverseBindMatrices":7,"skeleton":0,"joints":[0,1]}])"));
    auto staticJson=stream();writeGlbScene(staticJson,data.name,data.bones,data.meshes,{}, {},false,0,{});
    check(staticJson.str().find(R"("scenes":[{"nodes":[0]}])")!=std::string::npos);
    check(staticJson.str().find(R"("children":[1,2])")!=std::string::npos);
    check(staticJson.str().find("\"skin\"")==std::string::npos);
    check(staticJson.str().find("iiActorObjectExternalDocument")==std::string::npos);
}
