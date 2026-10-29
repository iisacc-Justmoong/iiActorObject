#include "Object3DFbxAnimation.h"
#include "Object3DProgressiveMorphCore.h"
#include "CharacterMath.h"
#include <iiActorObject/Animation.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace iiActorObject::object3d_detail
{
namespace
{
constexpr std::int64_t ticksPerSecond = 46186158000LL;

std::int64_t timeTick(double seconds)
{
    const long double result = static_cast<long double>(seconds) * ticksPerSecond;
    if (!std::isfinite(result) || result < 0 ||
        result >= static_cast<long double>(std::numeric_limits<std::int64_t>::max()) - .5L)
        throw std::invalid_argument("Animation time exceeds FBX range");
    return static_cast<std::int64_t>(std::floor(result + .5L));
}
float number(double value, bool nonzero = false)
{
    const float result = static_cast<float>(value);
    if (!std::isfinite(result) || (nonzero && result == 0))
        throw std::invalid_argument("Animation value is not representable as an FBX float");
    return result;
}
std::array<float,3> vector(Vector3 v, bool nonzero = false)
{
    return {number(v.x,nonzero),number(v.y,nonzero),number(v.z,nonzero)};
}
template<class Keys> std::vector<std::int64_t> keyTimes(const Keys &keys, double duration,
                                                        std::int64_t stop, std::size_t limit)
{
    if(limit==0)throw std::length_error("FBX animation sample budget exceeded");
    std::vector<std::int64_t> times{0};
    const auto append=[&](std::int64_t tick)
    {
        if(times.size()>=limit)throw std::length_error("FBX animation sample budget exceeded");
        times.push_back(tick);
    };
    for (const auto &key : keys)
    {
        const auto tick = timeTick(key.seconds);
        if ((key.seconds > 0 && tick == 0) || (key.seconds < duration && tick >= stop))
            throw std::invalid_argument("Animation keys collide with FBX duration boundaries");
        if (key.seconds == 0) continue;
        if (tick <= times.back()) throw std::invalid_argument("Animation keys collide at FBX time precision");
        append(tick);
    }
    if (times.back() < stop) append(stop);
    return times;
}
template<class Keys, class Interpolate>
auto sample(const Keys &keys, double time, Interpolate interpolate)
{
    if (time <= keys.front().seconds) return keys.front().value;
    if (time >= keys.back().seconds) return keys.back().value;
    const auto right = std::upper_bound(keys.begin(),keys.end(),time,
        [](double t,const auto &key){ return t < key.seconds; });
    const auto &left = *(right - 1);
    return interpolate(left.value,right->value,(time-left.seconds)/(right->seconds-left.seconds));
}
Quaternion fromEuler(const std::array<float,3> &angle)
{
    const auto rad = std::numbers::pi / 180.;
    const auto x = Quaternion::fromAxisAngle({1,0,0},angle[0]*rad);
    const auto y = Quaternion::fromAxisAngle({0,1,0},angle[1]*rad);
    const auto z = Quaternion::fromAxisAngle({0,0,1},angle[2]*rad);
    return character_detail::product(z,character_detail::product(y,x));
}
double angularError(Quaternion a, Quaternion b)
{
    a=a.normalized(); b=b.normalized();
    const double dot=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
    if(dot<0) b={-b.x,-b.y,-b.z,-b.w};
    const auto distance=std::hypot(std::hypot(a.x-b.x,a.y-b.y),std::hypot(a.z-b.z,a.w-b.w));
    return 4*std::asin(std::min(1.,distance*.5));
}
std::array<float,3> euler(Quaternion q, const std::array<float,3> *previous)
{
    q=q.normalized();
    const double s=std::clamp(2*(q.w*q.y-q.z*q.x),-1.,1.);
    const double y=std::asin(s);
    double x,z;
    if (1-std::abs(s) < 1e-14)
    {
        const double r01=2*(q.x*q.y-q.z*q.w), r11=1-2*(q.x*q.x+q.z*q.z);
        z=previous?(*previous)[2]*std::numbers::pi/180:0;
        x=std::atan2(std::copysign(1.,s)*r01,r11)+std::copysign(1.,s)*z;
    }
    else
    {
        x=std::atan2(2*(q.w*q.x+q.y*q.z),1-2*(q.x*q.x+q.y*q.y));
        z=std::atan2(2*(q.w*q.z+q.x*q.y),1-2*(q.y*q.y+q.z*q.z));
    }
    const auto degrees=180/std::numbers::pi;
    std::array<double,3> a{x*degrees,y*degrees,z*degrees};
    std::array<double,3> b{a[0]+180,180-a[1],a[2]+180};
    if(previous)
    {
        double da=0,db=0;
        for(std::size_t i=0;i<3;++i)
        {
            a[i]+=360*std::round(((*previous)[i]-a[i])/360);
            b[i]+=360*std::round(((*previous)[i]-b[i])/360);
            da+=(a[i]-(*previous)[i])*(a[i]-(*previous)[i]);
            db+=(b[i]-(*previous)[i])*(b[i]-(*previous)[i]);
        }
        if(db<da) a=b;
    }
    return {number(a[0]),number(a[1]),number(a[2])};
}
std::vector<std::int64_t> subdivision(const std::vector<std::int64_t> &bounds,
                                      std::int64_t step, std::size_t limit)
{
    if(limit==0) throw std::length_error("FBX animation sample budget exceeded");
    std::vector<std::int64_t> result{bounds.front()};
    for(std::size_t i=1;i<bounds.size();++i)
    {
        const auto gap=bounds[i]-bounds[i-1];
        const auto pieces=gap/step+(gap%step!=0);
        if (static_cast<std::uint64_t>(pieces)>limit-result.size())
            throw std::length_error("FBX animation sample budget exceeded");
        auto tick=bounds[i-1];
        for(std::int64_t part=1;part<pieces;++part) { tick+=step; result.push_back(tick); }
        result.push_back(bounds[i]);
    }
    return result;
}
FbxTransformTrack transformTrack(const BoneTrack &track, double duration, std::int64_t stop,
                                 std::int64_t step, double tolerance, std::size_t limit)
{
    auto ticks=subdivision(keyTimes(track.keys,duration,stop,limit),step,limit);
    auto at=[&](double tick)
    {
        return sample(track.keys,tick/static_cast<double>(ticksPerSecond),character_detail::blend);
    };
    for(unsigned pass=0;pass<64;++pass)
    {
        FbxTransformTrack out{track.bone,{}}; out.samples.reserve(ticks.size());
        for(const auto tick:ticks)
        {
            const auto t=at(static_cast<double>(tick));
            const auto *previous=out.samples.empty()?nullptr:&out.samples.back().rotation;
            const auto angles=euler(t.rotation,previous);
            if(angularError(fromEuler(angles),t.rotation)>tolerance)
                throw std::invalid_argument("FBX Euler float cannot represent a source rotation within tolerance");
            out.samples.push_back({tick,vector(t.position),angles,vector(t.scale,true)});
        }
        std::vector<std::int64_t> additional;
        for(std::size_t i=1;i<out.samples.size();++i)
        {
            const auto &a=out.samples[i-1], &b=out.samples[i];
            const auto gap=b.tick-a.tick; bool split=false;
            for(const auto fraction:{.25,.5,.75})
            {
                std::array<float,3> angles;
                for(std::size_t axis=0;axis<3;++axis)
                    angles[axis]=number(std::lerp(static_cast<double>(a.rotation[axis]),
                                                   static_cast<double>(b.rotation[axis]),fraction));
                const auto expected=at(static_cast<double>(a.tick)+static_cast<double>(gap)*fraction).rotation;
                if(angularError(fromEuler(angles),expected)>tolerance) { split=true; break; }
            }
            if(split)
            {
                if(gap<2) throw std::invalid_argument("FBX time precision cannot represent rotation within tolerance");
                additional.push_back(a.tick+gap/2);
            }
        }
        if(additional.empty()) return out;
        if(additional.size()>limit-ticks.size()) throw std::length_error("FBX animation sample budget exceeded");
        ticks.insert(ticks.end(),additional.begin(),additional.end());
        std::sort(ticks.begin(),ticks.end());
    }
    throw std::length_error("FBX rotation sampling did not converge");
}
} // namespace

FbxAnimationPlan planFbxAnimation(const Object3DAsset &asset, const AnimationClip &clip,
    const FbxAnimationExportOptions &options, const FbxMorphChannels &channels,
    const FbxProgressiveChannels &progressive,bool progressiveMode)
{
    if(clip.skeleton().get()!=asset.skeleton().get()||
       (!asset.hasRig()&&(!progressiveMode||!clip.tracks().empty())))
        throw std::invalid_argument("FBX animation requires the asset's own skeleton");
    if(!clip.requiredDeformationBind().empty())
        throw std::invalid_argument("Bake the required deformation bind into the asset before FBX animation export");
    if(!std::isfinite(options.maximumSampleInterval) || options.maximumSampleInterval<=0 ||
       !std::isfinite(options.maximumRotationError) || options.maximumRotationError<1e-7 ||
       options.maximumRotationError>std::numbers::pi || options.maximumSamples==0 || options.maximumSamples>1000000 ||
       options.indexedBindings.size()>1'000'000)
        throw std::invalid_argument("Invalid FBX animation sampling limits");
    const auto stop=timeTick(clip.duration());
    if(stop==0) throw std::invalid_argument("FBX duration is shorter than one time tick");
    const long double requestedStep=static_cast<long double>(options.maximumSampleInterval)*ticksPerSecond;
    if(requestedStep<1) throw std::invalid_argument("FBX sample interval is shorter than one time tick");
    const auto step=requestedStep>=stop?stop:static_cast<std::int64_t>(std::floor(requestedStep));
    FbxAnimationPlan plan{stop,{},{}}; std::size_t used=0;
    for(const auto &track:clip.tracks())
    {
        auto planned=transformTrack(track,clip.duration(),stop,step,options.maximumRotationError,
                                    options.maximumSamples-used);
        used+=planned.samples.size(); plan.transforms.push_back(std::move(planned));
    }
    using Target=std::pair<std::size_t,std::size_t>;
    std::map<Target,std::size_t> mapping;
    std::vector<bool> curveUsed(clip.curves().size(),false);
    std::vector<std::vector<std::int64_t>> curveChannels(clip.curves().size());
    for(const auto &binding:options.indexedBindings)
    {
        const Target target{binding.mesh,binding.target};
        if(binding.curve>=clip.curves().size()||binding.mesh>=asset.data().meshes.size()||
           binding.target>=asset.data().meshes[binding.mesh].geometry.morphTargets.size()||
           !channels.contains(target))
            throw std::invalid_argument("FBX indexed morph binding is out of range");
        if(!mapping.emplace(target,binding.curve).second)
            throw std::invalid_argument("FBX morph has more than one binding on the same mesh");
        curveUsed[binding.curve]=true;
    }
    if(progressiveMode)for(std::size_t i=0;i<clip.curves().size();++i)
    {
        const std::string_view name=clip.curves()[i].name;
        if(!name.starts_with(Object3DProgressiveCurvePrefix))continue;
        const auto found=progressive.find(name);
        if(found==progressive.end()||found->second.empty())
            throw std::invalid_argument("FBX animation references an unknown progressive control");
        curveChannels[i]=found->second;curveUsed[i]=true;
    }
    if(options.exportMatchingMorphCurves)
    {
        std::map<std::string_view,std::vector<Target>,std::less<>> named;
        for(const auto &[target,ids]:channels)
            if(!ids.empty())named[asset.data().meshes[target.first].geometry.morphTargets[target.second].name].push_back(target);
        for(std::size_t i=0;i<clip.curves().size();++i)
        {
            const auto found=named.find(clip.curves()[i].name);
            if(curveUsed[i]||found==named.end())continue;
            for(const auto target:found->second)
                if(!mapping.emplace(target,i).second)
                    throw std::invalid_argument("FBX automatic morph binding conflicts with an explicit binding");
            curveUsed[i]=true;
        }
    }
    for(const auto &[target,curve]:mapping)
    {
        const auto &ids=channels.at(target);
        curveChannels[curve].insert(curveChannels[curve].end(),ids.begin(),ids.end());
    }
    for(std::size_t index=0;index<clip.curves().size();++index)
    {
        const auto &curve=clip.curves()[index];const auto &selected=curveChannels[index];
        if(selected.empty())
        {
            if(!options.allowDiscardScalarCurves)
                throw std::invalid_argument("FBX scalar curve has no explicitly enabled matching morph channel");
            continue;
        }
        const auto copies=selected.size();
        const auto ticks=keyTimes(curve.keys,clip.duration(),stop,(options.maximumSamples-used)/copies);
        if(ticks.size()>(options.maximumSamples-used)/copies)
            throw std::length_error("FBX animation sample budget exceeded");
        FbxMorphTrack planned{selected,{}};
        for(auto tick:ticks)
        {
            const auto value=sample(curve.keys,static_cast<double>(tick)/ticksPerSecond,
                [](double a,double b,double t){return std::lerp(a,b,t);});
            planned.samples.push_back({tick,number(value*100,value!=0)});
        }
        used+=ticks.size()*copies;plan.morphs.push_back(std::move(planned));
    }
    if(plan.transforms.empty() && plan.morphs.empty())
        throw std::invalid_argument("FBX animation has no exportable tracks");
    plan.sampleCount=used;
    return plan;
}
} // namespace iiActorObject::object3d_detail
