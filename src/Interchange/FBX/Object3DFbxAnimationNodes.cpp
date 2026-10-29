#include "Object3DFbxAnimationNodes.h"
#include "Object3DFbxSceneNodes.h"
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace iiActorObject::object3d_detail
{
using namespace fbx_binary;
using namespace fbx_scene_nodes;
namespace
{
constexpr std::int64_t FbxTicksPerSecond=46186158000LL;
static std::int64_t takeOriginTick(double seconds)
{
    if(!std::isfinite(seconds))throw std::invalid_argument("Nonfinite FBX take start");
    const long double ticks=static_cast<long double>(seconds)*FbxTicksPerSecond;
    const long double rounded=std::round(ticks),limit=std::ldexp(1.0L,63);
    if(!std::isfinite(rounded)||rounded < -limit||rounded >= limit)
        throw std::overflow_error("FBX take start exceeds tick range");
    const auto result=static_cast<std::int64_t>(rounded);
    if(seconds!=0&&result==0)throw std::invalid_argument("FBX take start is shorter than one time tick");
    return result;
}
static void validateTakeTimes(const object3d_detail::FbxAnimationPlan &plan,std::int64_t origin)
{
    if(plan.duration<=0||origin>std::numeric_limits<std::int64_t>::max()-plan.duration)
        throw std::overflow_error("FBX take end exceeds tick range");
    const auto end=origin+plan.duration;
    const auto startSeconds=static_cast<double>(origin)/FbxTicksPerSecond;
    const auto endSeconds=static_cast<double>(end)/FbxTicksPerSecond;
    if(!(endSeconds>startSeconds))throw std::invalid_argument("FBX take duration loses floating-point precision");
    const auto check=[&](const auto &samples)
    {
        double previous=-std::numeric_limits<double>::infinity();
        double previousEncoded=-std::numeric_limits<double>::infinity();
        for(const auto &sample:samples)
        {
            if(sample.tick<0||sample.tick>plan.duration)throw std::logic_error("FBX animation sample outside take");
            const auto relative=static_cast<double>(sample.tick)/FbxTicksPerSecond;
            const auto time=startSeconds+relative;
            const auto encodedTime=static_cast<double>(origin+sample.tick)/FbxTicksPerSecond;
            if(!std::isfinite(time)||!std::isfinite(encodedTime)||time<=previous||encodedTime<=previousEncoded||
               (sample.tick>0&&sample.tick<plan.duration&&
                (time<=startSeconds||time>=endSeconds||encodedTime<=startSeconds||encodedTime>=endSeconds)))
                throw std::invalid_argument("FBX take samples lose floating-point time precision");
            previous=time;previousEncoded=encodedTime;
        }
    };
    for(const auto &track:plan.transforms)check(track.samples);
    for(const auto &track:plan.morphs)check(track.samples);
}
} // namespace
void appendFbxAnimationNodes(const FbxAnimationPlan &animation,std::string_view name,
    std::size_t takeIndex,std::size_t takeCount,double startSeconds,std::span<const std::int64_t> bones,
    std::int64_t &next,Node &objects,Node &connections,Node &takes)
{
    if(takeCount==0||takeIndex>=takeCount)throw std::invalid_argument("Invalid FBX take index");
    for(const auto &track:animation.transforms)
        if(track.samples.empty()||track.bone>=bones.size())throw std::invalid_argument("Invalid FBX transform track");
    for(const auto &track:animation.morphs)
        if(track.samples.empty())throw std::invalid_argument("Invalid FBX morph track");
    const auto connect=[&](std::int64_t child,std::int64_t parent)
    {connections.children.push_back({"C",{prop("OO"),prop(child),prop(parent)}, {}});};
    const auto origin=takeOriginTick(startSeconds);validateTakeTimes(animation,origin);
    const auto end=origin+animation.duration;
    const auto stack=next++, layer=next++;
    const auto pTime=[](std::string_view name,std::int64_t tick)
    {return Node{"P",{prop(name),prop("KTime"),prop("Time"),prop(""),prop(tick)}, {}};};
    objects.children.push_back({"AnimationStack",{prop(stack),prop(objectName(name,"AnimStack")),prop("")},
        {{"Properties70",{},{pTime("LocalStart",origin),pTime("LocalStop",end),
                            pTime("ReferenceStart",origin),pTime("ReferenceStop",end)}}}});
    objects.children.push_back({"AnimationLayer",{prop(layer),prop(objectName("BaseLayer","AnimLayer")),prop("")},
        {{"Properties70",{},{pEnum("BlendMode",1),pNumber("Weight",100)}}}});
    connect(layer,stack);
    const auto connectProperty=[&](std::int64_t source,std::int64_t target,std::string_view property)
    {connections.children.push_back({"C",{prop("OP"),prop(source),prop(target),prop(property)}, {}});};
    const auto writeCurve=[&](std::int64_t node,std::string_view component,
                              const std::vector<std::int64_t> &times,const std::vector<float> &values)
    {
        if(times.empty() || times.size()!=values.size() || times.size()>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
            throw std::length_error("Invalid FBX curve size");
        const auto id=next++;
        const std::array<std::int32_t,1> flags{4}, refs{static_cast<std::int32_t>(times.size())};
        const std::array<float,4> attrs{};
        objects.children.push_back({"AnimationCurve",{prop(id),prop(objectName("","AnimCurve")),prop("")},
            {value("Default",prop(static_cast<double>(values.front()))),value("KeyVer",prop(4008)),
             value("KeyTime",array(times)),value("KeyValueFloat",array(values)),
             value("KeyAttrFlags",array(flags)),value("KeyAttrDataFloat",array(attrs)),value("KeyAttrRefCount",array(refs))}});
        connectProperty(id,node,component);
    };
    constexpr std::array<std::string_view,3> components{"d|X","d|Y","d|Z"};
    constexpr std::array<std::string_view,3> properties{"Lcl Translation","Lcl Rotation","Lcl Scaling"};
    constexpr std::array<std::string_view,3> names{"T","R","S"};
    for(const auto &track:animation.transforms)
    {
        for(std::size_t kind=0;kind<3;++kind)
        {
            const auto node=next++;
            auto valuesFor=[kind](const auto &sample)->const std::array<float,3>&
            {return kind==0?sample.position:(kind==1?sample.rotation:sample.scale);};
            const auto &defaults=valuesFor(track.samples.front());
            objects.children.push_back({"AnimationCurveNode",{prop(node),prop(objectName(names[kind],"AnimCurveNode")),prop("")},
                {{"Properties70",{},{pNumber(components[0],defaults[0]),pNumber(components[1],defaults[1]),pNumber(components[2],defaults[2])}}}});
            connect(node,layer);connectProperty(node,bones[track.bone],properties[kind]);
            for(std::size_t axis=0;axis<3;++axis)
            {
                std::vector<std::int64_t> times;std::vector<float> values;
                times.reserve(track.samples.size());values.reserve(track.samples.size());
                for(const auto &sample:track.samples){times.push_back(origin+sample.tick);values.push_back(valuesFor(sample)[axis]);}
                writeCurve(node,components[axis],times,values);
            }
        }
    }
    for(const auto &track:animation.morphs)
    {
        std::vector<std::int64_t> times;std::vector<float> values;
        times.reserve(track.samples.size());values.reserve(track.samples.size());
        for(const auto &sample:track.samples){times.push_back(origin+sample.tick);values.push_back(sample.value);}
        for(const auto channel:track.channels)
        {
            const auto node=next++;
            objects.children.push_back({"AnimationCurveNode",{prop(node),prop(objectName("DeformPercent","AnimCurveNode")),prop("")},
                {{"Properties70",{},{pNumber("d|DeformPercent",values.front())}}}});
            connect(node,layer);connectProperty(node,channel,"DeformPercent");
            writeCurve(node,"d|DeformPercent",times,values);
        }
    }
    const auto filename=takeCount==1&&name=="Take"?
        std::string("Take.tak"):"Take_"+std::to_string(takeIndex)+".tak";
    takes.children.push_back({"Take",{prop(name)},{value("FileName",prop(filename)),
            {"LocalTime",{prop(origin),prop(end)},{}},
            {"ReferenceTime",{prop(origin),prop(end)},{}}}});
}
} // namespace iiActorObject::object3d_detail
