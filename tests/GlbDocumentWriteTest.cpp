#include "../src/Interchange/GLB/Object3DGlbDocumentWrite.h"
#include "../src/Interchange/GLB/Object3DGlbBuffer.h"
#include <iiActorObject/Object3DMaterial.h>
#include <iiActorObject/Object3DMesh.h>
#include <array>
#include <stdexcept>
#include <string>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail::glb_export;
namespace
{
void check(bool v){if(!v)throw std::runtime_error("GLB document writer contract failed");}
template<class F>void rejects(F f){try{f();}catch(const std::exception &){return;}throw std::runtime_error("Invalid GLB document accepted");}
}
int main()
{
    std::vector<Object3DMaterial> materials(1);const std::array<GlbMaterialVariant,1> variants{{{0,0,0}}};
    Buffer buffer;buffer.bytes={1,2,3};buffer.views={R"({"buffer":0,"byteOffset":0,"byteLength":3})"};
    GlbDocumentWriteInput input;input.objectName="Static";input.materials=materials;input.materialVariants=variants;
    const auto document=writeGlbDocumentJson(buffer,input);
    check(document.find(R"("asset":{"version":"2.0")")!=document.npos);
    check(document.find(R"("nodes":[{"name":"Static","children":[])")!=document.npos);
    check(document.find(R"("meshes":[],"materials":[)")!=document.npos);
    check(document.find(R"("bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":3}],"accessors":[],"buffers":[{"byteLength":3}])")!=document.npos);
    check(buffer.bytes==std::vector<std::uint8_t>({1,2,3}));
    input.animationJson=R"({"name":"Walk"})";
    check(writeGlbDocumentJson(buffer,input).find(R"("animations":[{"name":"Walk"}])")!=document.npos);
    const std::array<GlbMaterialVariant,2> extra{{{0,0,0},{0,1,0}}};
    input.externalDocument="id";input.materialVariants=extra;
    rejects([&]{(void)writeGlbDocumentJson(buffer,input);});
    input.materialVariants=variants;std::vector<Object3DMesh> meshes(1);input.meshes=meshes;
    rejects([&]{(void)writeGlbDocumentJson(buffer,input);});
}
