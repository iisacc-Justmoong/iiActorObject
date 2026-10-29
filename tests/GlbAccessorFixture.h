#pragma once
#include <iiActorObject/GlbDocument.h>
#include <bit>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace glb_test
{
inline void u32(std::vector<std::uint8_t> &out,std::uint32_t value)
{for(unsigned i=0;i<4;++i)out.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
inline iiActorObject::GlbDocument fixture()
{
    std::vector<std::uint8_t> bin;
    for(float value:{0.f,0.f,0.f,1.f,2.f,3.f})u32(bin,std::bit_cast<std::uint32_t>(value));
    bin.insert(bin.end(),{0,0,127,0,0,127,0,0});
    for(int i=0;i<8;++i){const std::int16_t value=(i%4==0||i%4==3?32767:0);bin.push_back(value&255);bin.push_back((value>>8)&255);}
    bin.insert(bin.end(),{0,255,128,0});
    bin.insert(bin.end(),{0,1,0,0,1,0,0,0});
    bin.insert(bin.end(),{255,0,0,0,255,0,0,0});
    bin.insert(bin.end(),{0,0,0,0,1,0,0,0});
    bin.insert(bin.end(),{0,0,1,0,0,0,0,0});
    if(bin.size()!=84)throw std::runtime_error("Invalid accessor fixture size");
    std::string json=R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":84}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":24},{"buffer":0,"byteOffset":24,"byteLength":6},{"buffer":0,"byteOffset":32,"byteLength":16},{"buffer":0,"byteOffset":48,"byteLength":4},{"buffer":0,"byteOffset":52,"byteLength":8},{"buffer":0,"byteOffset":60,"byteLength":8},{"buffer":0,"byteOffset":68,"byteLength":8},{"buffer":0,"byteOffset":76,"byteLength":6}],"accessors":[{"bufferView":0,"componentType":5126,"count":2,"type":"VEC3"},{"bufferView":1,"componentType":5120,"normalized":true,"count":2,"type":"VEC3"},{"bufferView":2,"componentType":5122,"normalized":true,"count":2,"type":"VEC4"},{"bufferView":3,"componentType":5121,"normalized":true,"count":2,"type":"VEC2"},{"bufferView":4,"componentType":5121,"count":2,"type":"VEC4"},{"bufferView":5,"componentType":5121,"normalized":true,"count":2,"type":"VEC4"},{"bufferView":6,"componentType":5123,"count":2,"type":"VEC2"},{"bufferView":7,"componentType":5123,"count":3,"type":"SCALAR"}]})";
    while(json.size()%4)json.push_back(' ');
    std::vector<std::uint8_t> bytes;u32(bytes,0x46546c67);u32(bytes,2);u32(bytes,static_cast<std::uint32_t>(28+json.size()+bin.size()));
    u32(bytes,static_cast<std::uint32_t>(json.size()));u32(bytes,0x4e4f534a);bytes.insert(bytes.end(),json.begin(),json.end());
    u32(bytes,static_cast<std::uint32_t>(bin.size()));u32(bytes,0x004e4942);bytes.insert(bytes.end(),bin.begin(),bin.end());
    return iiActorObject::GlbDocument::parse(bytes);
}
}
