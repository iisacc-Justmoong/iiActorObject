#include "../src/Interchange/GLB/Object3DGlbBuffer.h"
#include <iiActorObject/MeshData.h>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail::glb_export;
namespace
{
void check(bool value)
{if(!value)throw std::runtime_error("GLB buffer contract failed");}
template<class F>void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid GLB buffer input was accepted");}
}
int main()
{
    check(quote("a\"\\\n") == "\"a\\\"\\\\\\u000a\"");
    auto json=stream();json<<1.25;check(json.str()=="1.25");
    auto arrayJson=stream();const std::array<std::string,2> entries{"1","\"two\""};
    jsonArray(arrayJson,entries);check(arrayJson.str()=="[1,\"two\"]");
    std::vector<std::uint8_t> bytes;
    u16(bytes,0x1234);u32(bytes,0x89abcdef);f32(bytes,1.5);
    check(bytes==std::vector<std::uint8_t>({0x34,0x12,0xef,0xcd,0xab,0x89,0,0,0xc0,0x3f}));
    align(bytes);check(bytes.size()==12&&bytes[10]==0&&bytes[11]==0);
    rejects([&]{(void)narrow(std::numeric_limits<double>::infinity());});
    rejects([&]{(void)narrow(std::numeric_limits<double>::max());});

    Buffer buffer;
    check(buffer.view({1,2,3},34962)==0&&buffer.view({4,5})==1);
    check(buffer.bytes==std::vector<std::uint8_t>({1,2,3,0,4,5}));
    check(buffer.views[0]==R"({"buffer":0,"byteOffset":0,"byteLength":3,"target":34962})");
    check(buffer.views[1]==R"({"buffer":0,"byteOffset":4,"byteLength":2})");
    check(buffer.accessor(0,5126,1,"SCALAR")==0);
    check(buffer.accessors[0]==R"({"bufferView":0,"componentType":5126,"count":1,"type":"SCALAR"})");
    rejects([&]{(void)buffer.accessor(2,5126,1,"SCALAR");});

    Buffer vectors;
    const std::array<Vector3,2> points{{{-1,2,3},{4,-5,6}}};
    check(vectors.vectors(points,true)==0&&vectors.bytes.size()==24);
    check(vectors.accessors[0].find(R"("min":[-1,-5,3],"max":[4,2,6])")!=std::string::npos);

    Buffer sparse;
    MorphTarget morph{"Smile",{{2,{3,0,-1},{}},{0,{1,2,0},{}}}};
    check(sparse.sparse(morph,3,false)==0);
    check(sparse.bytes.size()==32&&sparse.bytes[0]==0&&sparse.bytes[4]==2);
    check(sparse.accessors[0].find(R"("min":[0,0,-1],"max":[3,2,0])")!=std::string::npos);
    check(sparse.accessors[0].find(R"("sparse":{"count":2)")!=std::string::npos);
    Buffer dense;
    MorphTarget complete{"Dense",{{1,{2,3,4},{}},{0,{1,2,3},{}}}};
    (void)dense.sparse(complete,2,false);
    check(dense.accessors[0].find(R"("min":[1,2,3],"max":[2,3,4])")!=std::string::npos);
    const auto previous=sparse.bytes;
    morph.deltas[0].vertex=3;rejects([&]{(void)sparse.sparse(morph,3,false);});
    morph.deltas[0].vertex=0;rejects([&]{(void)sparse.sparse(morph,3,false);});
    check(sparse.bytes==previous&&sparse.accessors.size()==1&&sparse.views.size()==2);
}
