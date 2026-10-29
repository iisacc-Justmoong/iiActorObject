#include "../src/Interchange/GLB/Object3DGlbSkinWrite.h"
#include "../src/Interchange/GLB/Object3DGlbBuffer.h"
#include <iiActorObject/Transform.h>
#include <array>
#include <bit>
#include <cstdint>
#include <stdexcept>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail::glb_export;
namespace
{
void check(bool v){if(!v)throw std::runtime_error("GLB inverse bind writer contract failed");}
float floatAt(const std::vector<std::uint8_t> &bytes,std::size_t offset)
{
    std::uint32_t value=0;for(unsigned i=0;i<4;++i)value|=std::uint32_t(bytes.at(offset+i))<<(8*i);
    return std::bit_cast<float>(value);
}
}
int main()
{
    Transform translated;translated.position={1,2,3};
    const std::array matrices{Matrix4{},translated.matrix()};Buffer buffer;
    check(writeGlbInverseBindMatrices(buffer,matrices)==0&&buffer.bytes.size()==128);
    check(buffer.accessors[0]==R"({"bufferView":0,"componentType":5126,"count":2,"type":"MAT4"})");
    check(floatAt(buffer.bytes,0)==1&&floatAt(buffer.bytes,60)==1&&floatAt(buffer.bytes,64+48)==1&&
          floatAt(buffer.bytes,64+52)==2&&floatAt(buffer.bytes,64+56)==3);
    try{(void)writeGlbInverseBindMatrices(buffer,{});throw std::runtime_error("Empty inverse bind accepted");}
    catch(const std::invalid_argument &){}
    check(buffer.bytes.size()==128&&buffer.accessors.size()==1);
}
