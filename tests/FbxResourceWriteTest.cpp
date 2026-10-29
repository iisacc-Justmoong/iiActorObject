#include "../src/Interchange/FBX/Object3DFbxResourceWrite.h"
#include <bit>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
using namespace iiActorObject::object3d_detail::fbx_binary;

namespace
{
void check(bool value)
{
    if(!value)throw std::runtime_error("FBX resource writer contract failed");
}
std::string stringValue(const Property &p)
{
    check(p.bytes.size()>=5&&p.bytes[0]=='S');
    std::uint32_t length=0;
    for(unsigned i=0;i<4;++i)length|=std::uint32_t(p.bytes[i+1])<<(8*i);
    check(p.bytes.size()==length+5);
    return {p.bytes.begin()+5,p.bytes.end()};
}
std::int64_t idValue(const Property &p)
{
    check(p.bytes.size()==9&&p.bytes[0]=='L');std::uint64_t value=0;
    for(unsigned i=0;i<8;++i)value|=std::uint64_t(p.bytes[i+1])<<(8*i);
    return std::bit_cast<std::int64_t>(value);
}
double numberValue(const Property &p)
{
    check(p.bytes.size()==9&&p.bytes[0]=='D');std::uint64_t value=0;
    for(unsigned i=0;i<8;++i)value|=std::uint64_t(p.bytes[i+1])<<(8*i);
    return std::bit_cast<double>(value);
}
const Node &find(const Node &parent,std::string_view name)
{
    for(const auto &child:parent.children)if(child.name==name)return child;
    throw std::runtime_error("Expected FBX node missing");
}
const Node &property(const Node &node,std::string_view name)
{
    const auto &properties=find(node,"Properties70");
    for(const auto &child:properties.children)
        if(child.name=="P"&&!child.properties.empty()&&stringValue(child.properties[0])==name)return child;
    throw std::runtime_error("Expected FBX property missing");
}
template<class F> void rejects(F operation)
{
    try {operation();}
    catch(const std::exception &) {return;}
    throw std::runtime_error("Invalid FBX resource input was accepted");
}
}
int main(int argc,char **argv)
{
    check(argc==2);
    std::ifstream in(argv[1],std::ios::binary);check(bool(in));
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in),{}};
    std::vector<Object3DImage> images;images.emplace_back("Checker",bytes,"image/png");
    Object3DMaterial opaque;opaque.name="Opaque";opaque.baseColor[3]=.3;
    opaque.surface={Object3DAlphaMode::Opaque,.25,true};opaque.baseColorImage=0;opaque.normalImage=0;
    opaque.baseColorUvSet="PaintUV";opaque.normalUvSet="PaintUV";opaque.normalScale=.6;
    Object3DMaterial blend;blend.name="Blend";blend.baseColor[3]=.25;
    std::vector<Object3DMaterial> materials{opaque,blend};
    Node objects{"Objects",{},{}},connections{"Connections",{}, {}};std::int64_t next=100;
    const auto ids=writeFbxResources(images,materials,true,next,objects,connections);
    check(ids==std::vector<std::int64_t>({102,104})&&next==105);
    check(objects.children.size()==5&&objects.children[0].name=="Video"&&objects.children[1].name=="Texture");
    check(objects.children[2].name=="Material"&&objects.children[3].name=="Texture"&&objects.children[4].name=="Material");
    check(find(objects.children[0],"Content").properties[0].bytes==blob(bytes).bytes);
    check(stringValue(property(objects.children[3],"UVSet").properties[4])=="PaintUV");
    check(numberValue(property(objects.children[2],"Opacity").properties[4])==1.0);
    check(numberValue(property(objects.children[4],"Opacity").properties[4])==.25);
    check(numberValue(property(objects.children[2],"iiBaseColorAlpha").properties[4])==.3);
    check(numberValue(property(objects.children[2],"iiNormalScale").properties[4])==.6);
    (void)property(objects.children[2],"iiActorObjectSourceMaterial");
    check(connections.children.size()==4);
    check(stringValue(connections.children[0].properties[0])=="OO");
    check(idValue(connections.children[0].properties[1])==100&&idValue(connections.children[0].properties[2])==101);
    check(idValue(connections.children[1].properties[1])==100&&idValue(connections.children[1].properties[2])==103);
    check(idValue(connections.children[2].properties[1])==103&&idValue(connections.children[2].properties[2])==102);
    check(idValue(connections.children[3].properties[1])==103&&idValue(connections.children[3].properties[2])==102);
    check(stringValue(connections.children[2].properties[3])=="DiffuseColor"&&stringValue(connections.children[3].properties[3])=="NormalMap");
    Node emptyObjects{"Objects",{},{}},emptyConnections{"Connections",{}, {}};std::int64_t emptyNext=7;
    check(writeFbxResources({}, {}, false,emptyNext,emptyObjects,emptyConnections).empty());
    check(emptyNext==7&&emptyObjects.children.empty()&&emptyConnections.children.empty());
    auto invalid=materials;invalid[0].baseColorImage=9;
    rejects([&]{Node a{"Objects",{},{}},b{"Connections",{}, {}};std::int64_t id=0;(void)writeFbxResources(images,invalid,false,id,a,b);});
    rejects([&]{Node a{"Objects",{},{}},b{"Connections",{}, {}};std::int64_t id=0;
        const std::vector<Object3DImage> missing(1);(void)writeFbxResources(missing,{},false,id,a,b);});
    rejects([&]{Node a{"Objects",{},{}},b{"Connections",{}, {}};std::int64_t id=std::numeric_limits<std::int64_t>::max();
        (void)writeFbxResources(images,{},false,id,a,b);});
}
