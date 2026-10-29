#include "../src/Interchange/GLB/Object3DGlbContainerWrite.h"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

using namespace iiActorObject::object3d_detail::glb_export;
namespace
{
void check(bool v){if(!v)throw std::runtime_error("GLB container writer contract failed");}
std::uint32_t u32(const std::vector<std::uint8_t> &bytes,std::size_t offset)
{std::uint32_t result=0;for(unsigned i=0;i<4;++i)result|=std::uint32_t(bytes.at(offset+i))<<(8*i);return result;}
}
int main()
{
    const std::string json=R"({"x":1})";const std::array<std::uint8_t,3> binary{1,2,3};
    const auto glb=writeGlbContainer(json,binary);
    check(glb.size()==40&&u32(glb,0)==0x46546c67&&u32(glb,4)==2&&u32(glb,8)==40);
    check(u32(glb,12)==8&&u32(glb,16)==0x4e4f534a);
    check(std::string(glb.begin()+20,glb.begin()+28)==R"({"x":1} )");
    check(u32(glb,28)==4&&u32(glb,32)==0x004e4942);
    check(std::vector<std::uint8_t>(glb.begin()+36,glb.end())==std::vector<std::uint8_t>({1,2,3,0}));
    check(binary==std::array<std::uint8_t,3>{1,2,3}&&json==R"({"x":1})");
    const auto aligned=writeGlbContainer("{}  ",std::array<std::uint8_t,4>{4,5,6,7});
    check(aligned.size()==36&&u32(aligned,12)==4&&u32(aligned,24)==4&&aligned.back()==7);
}
