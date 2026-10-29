#include "../src/Interchange/FBX/Object3DFbxAnimationNodes.h"
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
using namespace iiActorObject::object3d_detail::fbx_binary;

namespace
{
void check(bool condition)
{
    if(!condition)throw std::runtime_error("FBX animation node contract failed");
}
template<class F> void rejects(F operation)
{
    try { operation(); }
    catch(const std::exception &) { return; }
    throw std::runtime_error("Invalid FBX animation node data was accepted");
}
const Node &first(const Node &parent,std::string_view name)
{
    for(const auto &node:parent.children)if(node.name==name)return node;
    throw std::runtime_error("Expected FBX animation node was absent");
}
std::size_t count(const Node &parent,std::string_view name)
{
    std::size_t result=0;for(const auto &node:parent.children)if(node.name==name)++result;return result;
}
FbxAnimationPlan example()
{
    constexpr std::int64_t second=46'186'158'000;
    FbxAnimationPlan plan;
    plan.duration=second;
    plan.transforms.push_back({1,{{0,{0,1,2},{0,0,0},{1,1,1}},
        {second,{3,4,5},{0,0,90},{2,2,2}}}});
    plan.morphs.push_back({{33,34},{{0,0},{second,100}}});
    plan.sampleCount=4;
    return plan;
}
void append(const FbxAnimationPlan &plan,double start,Node &objects,Node &connections,Node &takes)
{
    std::int64_t next=1000;
    const std::array<std::int64_t,2> bones{20,21};
    appendFbxAnimationNodes(plan,"Walk",0,1,start,bones,next,objects,connections,takes);
}
}
int main()
{
    constexpr std::int64_t second=46'186'158'000;
    Node objects{"Objects",{},{}},connections{"Connections",{},{}},takes{"Takes",{}, {}};
    append(example(),2,objects,connections,takes);
    check(count(objects,"AnimationStack")==1&&count(objects,"AnimationLayer")==1);
    check(count(objects,"AnimationCurveNode")==5&&count(objects,"AnimationCurve")==11);
    check(connections.children.size()==22&&count(takes,"Take")==1);
    const auto &stack=first(objects,"AnimationStack");
    const auto &stackProps=first(stack,"Properties70");
    check(stackProps.children.size()==4);
    check(stackProps.children[0].properties[4].bytes==prop(2*second).bytes);
    check(stackProps.children[1].properties[4].bytes==prop(3*second).bytes);
    const auto &curve=first(objects,"AnimationCurve");
    const auto &times=first(curve,"KeyTime");
    const std::array<std::int64_t,2> expectedTimes{2*second,3*second};
    check(times.properties.at(0).bytes==array(expectedTimes).bytes);
    const auto &take=first(takes,"Take");
    check(take.properties.at(0).bytes==prop("Walk").bytes);
    check(first(take,"FileName").properties.at(0).bytes==prop("Take_0.tak").bytes);
    check(first(take,"LocalTime").properties[0].bytes==prop(2*second).bytes);
    check(first(take,"LocalTime").properties[1].bytes==prop(3*second).bytes);

    const auto bad=[&](FbxAnimationPlan plan,double start=0.)
    {
        Node a{"Objects",{},{}},b{"Connections",{},{}},c{"Takes",{}, {}};
        rejects([&]{append(plan,start,a,b,c);});
    };
    auto invalid=example();invalid.duration=0;bad(invalid);
    invalid=example();invalid.transforms[0].samples.clear();bad(invalid);
    invalid=example();invalid.morphs[0].samples.clear();bad(invalid);
    invalid=example();invalid.transforms[0].bone=2;bad(invalid);
    invalid=example();invalid.morphs[0].samples[1].tick=0;bad(invalid);
    invalid=example();invalid.transforms[0].samples[1].tick=second+1;bad(invalid);
    bad(example(),std::numeric_limits<double>::infinity());
    bad(example(),std::numeric_limits<double>::quiet_NaN());
    bad(example(),1e30);
    bad(example(),1e-20);
}
