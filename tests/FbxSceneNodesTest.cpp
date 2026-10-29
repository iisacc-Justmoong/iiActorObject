#include "../src/Interchange/FBX/Object3DFbxSceneNodes.h"
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail::fbx_binary;
using namespace iiActorObject::object3d_detail::fbx_scene_nodes;

namespace
{
void check(bool condition)
{
    if (!condition) throw std::runtime_error("FBX scene node contract failed");
}
std::string stringValue(const Property &p)
{
    check(p.bytes.size() >= 5 && p.bytes[0] == 'S');
    std::uint32_t length = 0;
    for (unsigned i=0; i<4; ++i) length |= std::uint32_t(p.bytes[1+i]) << (8*i);
    check(p.bytes.size() == 5 + length);
    return {p.bytes.begin()+5,p.bytes.end()};
}
double numberValue(const Property &p)
{
    check(p.bytes.size() == 9 && p.bytes[0] == 'D');
    std::uint64_t bits = 0;
    for (unsigned i=0; i<8; ++i) bits |= std::uint64_t(p.bytes[1+i]) << (8*i);
    return std::bit_cast<double>(bits);
}
std::int32_t integerValue(const Property &p)
{
    check(p.bytes.size() == 5 && p.bytes[0] == 'I');
    std::uint32_t bits = 0;
    for (unsigned i=0; i<4; ++i) bits |= std::uint32_t(p.bytes[1+i]) << (8*i);
    return std::bit_cast<std::int32_t>(bits);
}
void near(double actual,double expected)
{
    check(std::abs(actual-expected)<1e-10);
}
template<class F> void rejects(F operation)
{
    try { operation(); }
    catch (const std::exception &) { return; }
    throw std::runtime_error("Invalid FBX scene node input accepted");
}
}
int main()
{
    const auto name=objectName("Joint","Model");
    check(name==std::string("Joint\0\1Model",12));
    const auto integer=pInt("UpAxis",1);
    check(integer.name=="P" && integer.properties.size()==5);
    check(stringValue(integer.properties[0])=="UpAxis" && stringValue(integer.properties[1])=="int");
    check(integerValue(integer.properties[4])==1);
    const auto number=pNumber("Opacity",0.25);
    near(numberValue(number.properties.at(4)),0.25);
    const auto enumeration=pEnum("Mode",2);
    check(stringValue(enumeration.properties.at(1))=="enum" && integerValue(enumeration.properties.at(4))==2);
    const auto text=externalText("Source","alpha");
    check(stringValue(text.properties.at(1))=="KString" && stringValue(text.properties.at(3))=="U");
    check(stringValue(text.properties.at(4))=="alpha");
    check(integerValue(externalInteger("Source",-3).properties.at(4))==-3);
    near(numberValue(externalNumber("Amount",0.75).properties.at(4)),0.75);
    check(externalIndex(static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))==std::numeric_limits<std::int32_t>::max());
    rejects([] { (void)externalIndex(static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())+1); });

    Transform transform;
    transform.position={1,2,3};
    transform.scale={2,3,4};
    transform.rotation=Quaternion::fromAxisAngle({0,0,1},std::numbers::pi/2);
    const auto node=model(42,"Hand","LimbNode",transform);
    check(node.name=="Model" && node.properties.size()==3 && node.children.size()==3);
    check(stringValue(node.properties[1])==std::string("Hand\0\1Model",11));
    check(stringValue(node.properties[2])=="LimbNode");
    const auto &properties=node.children.at(1).children;
    check(properties.size()==5 && stringValue(properties[0].properties[0])=="Lcl Translation");
    near(numberValue(properties[0].properties[4]),1);near(numberValue(properties[0].properties[5]),2);near(numberValue(properties[0].properties[6]),3);
    near(numberValue(properties[1].properties[4]),0);near(numberValue(properties[1].properties[5]),0);near(numberValue(properties[1].properties[6]),90);
    near(numberValue(properties[2].properties[4]),2);near(numberValue(properties[2].properties[5]),3);near(numberValue(properties[2].properties[6]),4);
    check(integerValue(properties[3].properties[4])==0 && integerValue(properties[4].properties[4])==1);
    const auto layer=layerElement("LayerElementUV",2);
    check(layer.name=="LayerElement" && stringValue(layer.children.at(0).properties.at(0))=="LayerElementUV");
    check(integerValue(layer.children.at(1).properties.at(0))==2);
    rejects([] { Transform invalid; invalid.rotation={0,0,0,0}; (void)model(1,"Invalid","Null",invalid); });
}
