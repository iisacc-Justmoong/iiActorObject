#pragma once
#include <iiActorObject/Features.h>
#include <iiActorObject/Object3DInterchange.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#endif
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

inline int glbMixedConsumer()
{
    using namespace iiActorObject;
    Object3DData data;data.bones={{"root",NoBone,{}}};
    data.materials={Object3DMaterial{"A"},Object3DMaterial{"B"}};
    Object3DMesh mesh;mesh.name="Character";
    mesh.geometry.vertices={{{0,0,0},{0,0,1},{{0,1}}},{{1,0,0},{0,0,1},{{0,1}}},
                            {{0,1,0},{0,0,1},{{0,1}}},{{1,1,0},{0,0,1},{{0,1}}}};
    mesh.geometry.triangles={0,1,2,1,3,2};mesh.triangleMaterials={0,1};
    mesh.textureCoordinates={{0,0},{1,0},{0,1},{1,1}};data.meshes.push_back(std::move(mesh));
    Object3DEngine engine;const auto source=engine.createObject(std::move(data));const auto original=exportObject3DGlb(source.get());
    const auto u32=[&](std::size_t offset){return std::uint32_t(original[offset])|std::uint32_t(original[offset+1])<<8|
        std::uint32_t(original[offset+2])<<16|std::uint32_t(original[offset+3])<<24;};
    const auto jsonSize=u32(12);std::string json(reinterpret_cast<const char*>(original.data()+20),jsonSize);
    const auto first=json.find("\"attributes\":{");const auto second=json.find("\"attributes\":{",first+1);
    if(first==json.npos||second==json.npos)return 1;
    const auto removeAttribute=[&](std::string_view name)
    {
        const auto end=json.find('}',second),begin=json.find("\""+std::string(name)+"\":",second);
        if(begin==json.npos||begin>=end)throw std::runtime_error("consumer fixture attribute missing");
        auto last=begin+name.size()+3;while(last<end&&json[last]>='0'&&json[last]<='9')++last;
        if(begin>second&&json[begin-1]==',')json.erase(begin-1,last-begin+1);
        else if(last<end&&json[last]==',')json.erase(begin,last-begin+1);
        else throw std::runtime_error("consumer fixture attribute separator missing");
    };
    removeAttribute("NORMAL");removeAttribute("TEXCOORD_0");
    while(json.size()%4)json.push_back(' ');const auto binaryStart=20+jsonSize;
    std::vector<std::uint8_t> bytes;
    const auto put=[&](std::uint32_t n){for(unsigned i=0;i<4;++i)bytes.push_back(static_cast<std::uint8_t>(n>>(8*i)));};
    put(0x46546c67);put(2);put(static_cast<std::uint32_t>(20+json.size()+original.size()-binaryStart));
    put(static_cast<std::uint32_t>(json.size()));put(0x4e4f534a);bytes.insert(bytes.end(),json.begin(),json.end());
    bytes.insert(bytes.end(),original.begin()+static_cast<std::ptrdiff_t>(binaryStart),original.end());
#if IIACTOROBJECT_HAS_NSCHARACTER
    const auto result=NSCharacter::importGlb(engine.reference(),bytes);
    const auto object=result.asset->object();const auto &warnings=result.warnings;
#else
    const auto result=importObject3DGlb(engine.reference(),bytes);
    const auto object=result.object;const auto &warnings=result.warnings;
#endif
    const auto &loaded=object.get().data().meshes[0];
    return warnings.empty()||!loaded.topology||loaded.topology->points.size()!=4||
        loaded.geometry.triangles.size()!=6||loaded.textureCoordinates.size()!=loaded.geometry.vertices.size()?2:0;
}
