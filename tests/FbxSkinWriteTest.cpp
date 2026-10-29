#include "../src/Interchange/FBX/Object3DFbxSkinWrite.h"
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
using namespace iiActorObject::object3d_detail::fbx_binary;
namespace
{
void check(bool value)
{if(!value)throw std::runtime_error("FBX skin writer contract failed");}
template<class F> void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid FBX skin writer input was accepted");}
const Node &child(const Node &node,std::string_view name)
{for(const auto &value:node.children)if(value.name==name)return value;throw std::runtime_error("Missing FBX skin field");}
bool equal(const Node &a,const Node &b)
{
    if(a.name!=b.name||a.properties.size()!=b.properties.size()||a.children.size()!=b.children.size())return false;
    for(std::size_t i=0;i<a.properties.size();++i)if(a.properties[i].bytes!=b.properties[i].bytes)return false;
    for(std::size_t i=0;i<a.children.size();++i)if(!equal(a.children[i],b.children[i]))return false;
    return true;
}
struct Output final {Node objects{"Objects",{},{}},connections{"Connections",{}, {}};std::int64_t next=1000;};
}
int main()
{
    const std::array<FbxSkinBone,2> bones{{{"Root",200,Matrix4{},Matrix4{}},{"Arm",201,Matrix4{},Matrix4{}}}};
    const std::vector<TopologyControlPoint> points{{{},{{0,1}}},{{},{{1,1}}},{{},{{0,.25},{1,.75}}}};
    const SkinningProfile profile{SkinningMethod::Blended,{0,.5,1}};
    Output source;
    writeFbxSkin("Body",profile,std::span<const TopologyControlPoint>(points),bones,400,300,source.next,source.objects,source.connections);
    check(source.next==1002&&source.objects.children.size()==3&&source.connections.children.size()==5);
    const auto &skin=source.objects.children[0];
    check(skin.name=="Deformer"&&skin.properties[0].bytes==prop(std::int64_t{400}).bytes);
    check(child(skin,"SkinningType").properties[0].bytes==prop("Blend").bytes);
    check(child(skin,"Indexes").properties[0].bytes==array(std::array<std::int32_t,3>{0,1,2}).bytes);
    check(child(skin,"BlendWeights").properties[0].bytes==array(std::array<double,3>{0,.5,1}).bytes);
    const auto &cluster0=source.objects.children[1],&cluster1=source.objects.children[2];
    check(child(cluster0,"Indexes").properties[0].bytes==array(std::array<std::int32_t,2>{0,2}).bytes);
    check(child(cluster0,"Weights").properties[0].bytes==array(std::array<double,2>{1,.25}).bytes);
    check(child(cluster1,"Indexes").properties[0].bytes==array(std::array<std::int32_t,2>{1,2}).bytes);
    check(child(cluster1,"Weights").properties[0].bytes==array(std::array<double,2>{1,.75}).bytes);
    check(source.connections.children[0].properties[1].bytes==prop(std::int64_t{400}).bytes);
    check(source.connections.children[0].properties[2].bytes==prop(std::int64_t{300}).bytes);
    check(source.connections.children[2].properties[1].bytes==prop(std::int64_t{200}).bytes);
    check(source.connections.children[4].properties[1].bytes==prop(std::int64_t{201}).bytes);

    std::vector<SkinVertex> vertices(points.size());
    for(std::size_t i=0;i<points.size();++i)vertices[i].weights=points[i].weights;
    Output render;
    writeFbxSkin("Body",profile,std::span<const SkinVertex>(vertices),bones,400,300,render.next,render.objects,render.connections);
    check(equal(source.objects,render.objects)&&equal(source.connections,render.connections)&&source.next==render.next);
    Output linear;writeFbxSkin("Body",SkinningProfile{},std::span<const SkinVertex>(vertices),bones,400,300,linear.next,linear.objects,linear.connections);
    check(child(linear.objects.children[0],"SkinningType").properties[0].bytes==prop("Linear").bytes);
    Output dual;writeFbxSkin("Body",SkinningProfile{SkinningMethod::DualQuaternion,{}},std::span<const SkinVertex>(vertices),bones,400,300,dual.next,dual.objects,dual.connections);
    check(child(dual.objects.children[0],"SkinningType").properties[0].bytes==prop("DualQuaternion").bytes);
    auto invalidVertices=vertices;invalidVertices[0].weights[0].bone=2;
    rejects([&]{Output out;writeFbxSkin("Body",profile,std::span<const SkinVertex>(invalidVertices),bones,400,300,out.next,out.objects,out.connections);});
    invalidVertices=vertices;invalidVertices[0].weights[0].weight=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{Output out;writeFbxSkin("Body",profile,std::span<const SkinVertex>(invalidVertices),bones,400,300,out.next,out.objects,out.connections);});
    rejects([&]{Output out;writeFbxSkin("Body",SkinningProfile{SkinningMethod::Blended,{.5}},std::span<const SkinVertex>(vertices),bones,400,300,out.next,out.objects,out.connections);});
    rejects([&]{Output out;out.next=std::numeric_limits<std::int64_t>::max();writeFbxSkin("Body",profile,std::span<const SkinVertex>(vertices),bones,400,300,out.next,out.objects,out.connections);});
}
