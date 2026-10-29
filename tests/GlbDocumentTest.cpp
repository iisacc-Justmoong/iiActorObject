#include <iiActorObject/GlbDocument.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace iiActorObject;
#define CHECK(x) do { if(!(x))throw std::runtime_error(#x); } while(false)
template<class F> void rejects(F f){bool caught=false;try{f();}catch(const std::exception &){caught=true;}CHECK(caught);}
void u32(std::vector<std::uint8_t> &bytes,std::uint32_t value)
{for(unsigned i=0;i<4;++i)bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
void f32(std::vector<std::uint8_t> &bytes,float value){u32(bytes,std::bit_cast<std::uint32_t>(value));}
std::vector<std::uint8_t> glb(std::string json,std::vector<std::uint8_t> bin)
{
    while(json.size()%4)json.push_back(' ');while(bin.size()%4)bin.push_back(0);
    std::vector<std::uint8_t> result;u32(result,0x46546c67);u32(result,2);u32(result,static_cast<std::uint32_t>(28+json.size()+bin.size()));
    u32(result,static_cast<std::uint32_t>(json.size()));u32(result,0x4e4f534a);result.insert(result.end(),json.begin(),json.end());
    u32(result,static_cast<std::uint32_t>(bin.size()));u32(result,0x004e4942);result.insert(result.end(),bin.begin(),bin.end());return result;
}
std::string json()
{
    return R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":60}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8,"byteStride":4},{"buffer":0,"byteOffset":8,"byteLength":2},{"buffer":0,"byteOffset":12,"byteLength":24},{"buffer":0,"byteOffset":36,"byteLength":24}],"accessors":[{"bufferView":0,"componentType":5121,"normalized":true,"count":2,"type":"VEC2"},{"componentType":5126,"count":3,"type":"VEC3","sparse":{"count":2,"indices":{"bufferView":1,"componentType":5121},"values":{"bufferView":2}}},{"bufferView":3,"componentType":5123,"count":1,"type":"MAT3"}]})";
}
std::vector<std::uint8_t> binary()
{
    std::vector<std::uint8_t> result{0,255,88,88,255,128,88,88,0,2,0,0};
    for(float value:{1.f,2.f,3.f,-1.f,.5f,4.f})f32(result,value);
    for(unsigned column=0;column<3;++column){for(unsigned row=1;row<=3;++row){result.push_back(column*3+row);result.push_back(0);}result.push_back(0);result.push_back(0);}
    CHECK(result.size()==60);return result;
}
void replace(std::string &text,std::string_view from,std::string_view to)
{const auto pos=text.find(from);CHECK(pos!=std::string::npos);text.replace(pos,from.size(),to);}
void valid()
{
    auto bytes=glb(json(),binary());const auto document=GlbDocument::parse(bytes);
    CHECK(document.accessorCount()==3&&document.bufferViewCount()==4&&!document.json().empty());
    const auto a=document.accessor(0);CHECK(a.count==2&&a.components==2&&a.normalized&&a.component==GlbComponentType::UnsignedByte);
    const auto normalized=document.readFloats(0);CHECK(normalized.size()==4&&normalized[0]==0&&normalized[1]==1&&normalized[2]==1);
    CHECK(std::abs(normalized[3]-128./255.)<1e-12);rejects([&]{(void)document.readUnsigned(0);});
    const auto sparse=document.readFloats(1);CHECK(sparse==std::vector<double>({1,2,3,0,0,0,-1,.5,4}));
    const auto m=document.accessor(2);CHECK(m.components==9&&m.type==GlbAccessorType::Mat3);
    CHECK(document.readUnsigned(2)==std::vector<std::uint32_t>({1,2,3,4,5,6,7,8,9}));
    CHECK(document.bufferView(0).size()==8&&document.bufferView(0)[1]==255);
    std::fill(bytes.begin(),bytes.end(),0);CHECK(document.readFloats(1)==sparse);
    rejects([&]{(void)document.accessor(3);});rejects([&]{(void)document.readFloats(3);});rejects([&]{(void)document.bufferView(4);});

    const std::string overlayJson=R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":20}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":12,"byteStride":4},{"buffer":0,"byteOffset":12,"byteLength":1},{"buffer":0,"byteOffset":16,"byteLength":1}],"accessors":[{"bufferView":0,"componentType":5121,"count":3,"type":"SCALAR","sparse":{"count":1,"indices":{"bufferView":1,"componentType":5121},"values":{"bufferView":2}}}]})";
    const auto overlay=GlbDocument::parse(glb(overlayJson,{10,0,0,0,20,0,0,0,30,0,0,0,1,0,0,0,7,0,0,0}));
    CHECK(overlay.readUnsigned(0)==std::vector<std::uint32_t>({10,7,30}));
    CHECK(overlay.readFloats(0)==std::vector<double>({10,7,30}));

    const std::string numericJson=R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":20}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":3},{"buffer":0,"byteOffset":4,"byteLength":6},{"buffer":0,"byteOffset":12,"byteLength":4},{"buffer":0,"byteOffset":16,"byteLength":4}],"accessors":[{"bufferView":0,"componentType":5120,"normalized":true,"count":3,"type":"SCALAR"},{"bufferView":1,"componentType":5122,"normalized":true,"count":3,"type":"SCALAR"},{"bufferView":2,"componentType":5125,"count":1,"type":"SCALAR"},{"bufferView":3,"componentType":5126,"count":1,"type":"SCALAR"}]})";
    const std::vector<std::uint8_t> numericBytes{128,129,127,0,0,128,1,128,255,127,0,0,255,255,255,255,0,0,128,127};
    const auto numeric=GlbDocument::parse(glb(numericJson,numericBytes));
    CHECK(numeric.readFloats(0)==std::vector<double>({-1,-1,1}));
    CHECK(numeric.readFloats(1)==std::vector<double>({-1,-1,1}));
    CHECK(numeric.readUnsigned(2)==std::vector<std::uint32_t>({std::numeric_limits<std::uint32_t>::max()}));
    rejects([&]{(void)numeric.readFloats(3);});
}
void invalid()
{
    const auto malformed=[&](std::string text,std::vector<std::uint8_t> bytes=binary())
    {rejects([&]{(void)GlbDocument::parse(glb(std::move(text),std::move(bytes)));});};
    auto text=json();replace(text,"\"byteLength\":60","\"byteLength\":61");malformed(text);
    text=json();replace(text,"\"buffer\":0,\"byteOffset\":36,\"byteLength\":24","\"buffer\":0,\"byteOffset\":36,\"byteLength\":25");malformed(text);
    text=json();replace(text,"\"byteStride\":4","\"byteStride\":2");malformed(text);
    text=json();replace(text,"\"count\":3,\"type\":\"VEC3\"","\"count\":18446744073709551615,\"type\":\"VEC3\"");malformed(text);
    text=json();replace(text,"\"byteLength\":60","\"byteLength\":60,\"uri\":\"outside.bin\"");malformed(text);
    text=json();replace(text,"\"componentType\":5121,\"normalized\":true","\"componentType\":5126,\"normalized\":true");malformed(text);
    text=json();replace(text,"\"byteOffset\":12,\"byteLength\":24","\"byteOffset\":13,\"byteLength\":23");malformed(text);
    auto unsorted=binary();unsorted[8]=2;unsorted[9]=0;malformed(json(),unsorted);
    auto duplicate=binary();duplicate[8]=1;duplicate[9]=1;malformed(json(),duplicate);
    auto outside=binary();outside[9]=3;malformed(json(),outside);
    auto bytes=glb(json(),binary());bytes[0]=0;rejects([&]{(void)GlbDocument::parse(bytes);});
    bytes=glb(json(),binary());bytes.pop_back();rejects([&]{(void)GlbDocument::parse(bytes);});
    bytes=glb(json(),binary());bytes[4]=3;rejects([&]{(void)GlbDocument::parse(bytes);});
    auto options=GlbReadOptions{};options.maximumAccessorValues=16;rejects([&]{(void)GlbDocument::parse(glb(json(),binary()),options);});
    options={};options.maximumSparseEntries=1;rejects([&]{(void)GlbDocument::parse(glb(json(),binary()),options);});
    options={};options.maximumBytes=8;rejects([&]{(void)GlbDocument::parse(glb(json(),binary()),options);});
    options={};options.maximumAccessors=2;rejects([&]{(void)GlbDocument::parse(glb(json(),binary()),options);});
    options={};options.maximumBufferViews=3;rejects([&]{(void)GlbDocument::parse(glb(json(),binary()),options);});
    options={};options.maximumJsonBytes=32;rejects([&]{(void)GlbDocument::parse(glb(json(),binary()),options);});
    options={};options.maximumJsonItems=2;rejects([&]{(void)GlbDocument::parse(glb(json(),binary()),options);});
    text=json();replace(text,"\"version\":\"2.0\"","\"version\":\"1.0\"");malformed(text);
    text=json();replace(text,"\"count\":2,\"type\":\"VEC2\"","\"count\":2,\"type\":\"VEC5\"");malformed(text);
    text=json();replace(text,"\"bufferView\":0,\"componentType\":5121","\"bufferView\":8,\"componentType\":5121");malformed(text);
}
void mutations()
{
    const auto seed=glb(json(),binary());std::mt19937 random(0x47544232);
    GlbReadOptions options;options.maximumBytes=2048;options.maximumJsonBytes=1024;
    options.maximumJsonItems=500;options.maximumAccessorValues=1000;options.maximumSparseEntries=100;
    std::size_t accepted=0,rejected=0;
    for(std::size_t round=0;round<2500;++round)
    {
        auto input=seed;
        const auto edits=1+random()%5;
        for(unsigned n=0;n<edits;++n)input[random()%input.size()]^=static_cast<std::uint8_t>(1u<<(random()%8));
        try
        {
            const auto document=GlbDocument::parse(input,options);
            for(std::size_t i=0;i<document.bufferViewCount();++i)(void)document.bufferView(i);
            for(std::size_t i=0;i<document.accessorCount();++i)
            {
                const auto a=document.accessor(i);(void)document.readFloats(i);
                if(!a.normalized&&(a.component==GlbComponentType::UnsignedByte||a.component==GlbComponentType::UnsignedShort||a.component==GlbComponentType::UnsignedInt))
                    (void)document.readUnsigned(i);
            }
            ++accepted;
        }
        catch(const std::exception&){++rejected;}
    }
    CHECK(accepted>0&&rejected>0);
}
int main(int argc,char **argv)
{
    try{if(argc!=2)return 2;const std::string_view mode=argv[1];if(mode=="valid")valid();else if(mode=="invalid")invalid();else if(mode=="mutations")mutations();else return 2;std::cout<<mode<<" passed\n";return 0;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
