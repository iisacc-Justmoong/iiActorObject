#include <iiActorObject/GlbAnimationBake.h>
#include "CharacterMath.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <numbers>
#include <set>
#include <stdexcept>
#include <unordered_map>

namespace iiActorObject
{
namespace
{
constexpr long double FbxTicksPerSecond=46186158000.L;
struct Interval final {double first,last;std::size_t depth;};
[[noreturn]] void fail(std::string_view message)
{throw std::invalid_argument("GLB animation bake: "+std::string(message));}
void validate(const GlbAnimationBakeOptions &o)
{
    const auto positive=[](double v){return std::isfinite(v)&&v>0;};
    if(!positive(o.maximumSampleInterval)||!positive(o.maximumPositionError)||!positive(o.maximumRotationError)||
       o.maximumRotationError>std::numbers::pi||!positive(o.maximumScaleError)||!positive(o.maximumMorphError)||
       !std::isfinite(o.maximumMatrixError)||o.maximumMatrixError<0||o.maximumMatrixError>.01||
       !positive(o.staticDuration)||!positive(o.maximumStepTransition)||
       (o.steps!=GlbStepConversion::Reject&&o.steps!=GlbStepConversion::Approximate)||
       o.maximumSampleTimes<2||o.maximumSampleTimes>1'000'000||
       o.maximumEvaluations<2||o.maximumEvaluations>4'000'000||
       !o.maximumOutputKeys||o.maximumOutputKeys>100'000'000||
       !o.maximumCachedValues||o.maximumCachedValues>100'000'000||
       !o.maximumNodeVisits||o.maximumNodeVisits>1'000'000'000||
       !o.maximumChannelValueVisits||o.maximumChannelValueVisits>1'000'000'000||
       !o.maximumSourceKeyVisits||o.maximumSourceKeyVisits>100'000'000||o.maximumSubdivisionDepth>48)
        fail("invalid limits or options");
}
struct TimeCode final {float glb;std::int64_t fbx;};
TimeCode portableTime(double time)
{
    const auto glb=static_cast<float>(time);
    const auto ticks=static_cast<long double>(time)*FbxTicksPerSecond;
    if(!std::isfinite(glb)||glb<0||(time>0&&glb==0)||!std::isfinite(ticks)||
       ticks>=static_cast<long double>(std::numeric_limits<std::int64_t>::max())-.5L)
        fail("time cannot be represented in GLB and FBX");
    return {glb,static_cast<std::int64_t>(std::floor(ticks+.5L))};
}
void addTime(std::set<double> &times,double time,const GlbAnimationBakeOptions &options)
{
    const auto next=times.lower_bound(time);
    if(next!=times.end()&&*next==time)return;
    if(times.size()>=options.maximumSampleTimes)throw std::length_error("GLB animation bake sample budget exceeded");
    if(options.requirePortableTimes)
    {
        const auto code=portableTime(time);
        if(next!=times.end())
        {const auto after=portableTime(*next);if(code.glb>=after.glb||code.fbx>=after.fbx)fail("sample times collide in GLB or FBX");}
        if(next!=times.begin())
        {const auto before=portableTime(*std::prev(next));if(code.glb<=before.glb||code.fbx<=before.fbx)fail("sample times collide in GLB or FBX");}
    }
    times.insert(next,time);
}
double beforeStep(double at,const GlbAnimationBakeOptions &options)
{
    double before=std::nextafter(at,-std::numeric_limits<double>::infinity());
    if(options.requirePortableTimes)
    {
        const double target=std::min(before,at-static_cast<double>(2/FbxTicksPerSecond));
        if(!(target>0))fail("STEP transition is too close to the start for portable timing");
        auto f=static_cast<float>(target),end=static_cast<float>(at);
        if(!std::isfinite(f)||!std::isfinite(end))fail("STEP transition exceeds portable timing");
        if(static_cast<double>(f)>target)f=std::nextafter(f,-std::numeric_limits<float>::infinity());
        if(f>=end)f=std::nextafter(end,-std::numeric_limits<float>::infinity());
        before=static_cast<double>(f);
    }
    if(!(before>0&&before<at)||at-before>options.maximumStepTransition||
       at-before>options.maximumSampleInterval)
        fail("STEP transition cannot fit within the requested timing limit");
    return before;
}
double rotationError(Quaternion a,Quaternion b)
{
    a=a.normalized();b=b.normalized();
    if(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w<0)b={-b.x,-b.y,-b.z,-b.w};
    const auto distance=std::hypot(std::hypot(a.x-b.x,a.y-b.y),std::hypot(a.z-b.z,a.w-b.w));
    return 4*std::asin(std::min(1.,distance*.5));
}
double distance(Vector3 a,Vector3 b)
{return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);}
void maximum(GlbBakeErrors &a,const GlbBakeErrors &b)
{a.position=std::max(a.position,b.position);a.rotation=std::max(a.rotation,b.rotation);
 a.scale=std::max(a.scale,b.scale);a.morph=std::max(a.morph,b.morph);}
bool within(const GlbBakeErrors &e,const GlbAnimationBakeOptions &o)
{return e.position<=o.maximumPositionError&&e.rotation<=o.maximumRotationError&&
        e.scale<=o.maximumScaleError&&e.morph<=o.maximumMorphError;}
bool stepChanged(const GlbAnimationChannel &channel,std::size_t key)
{
    const auto first=(key-1)*channel.components,second=key*channel.components;
    if(channel.path==GlbAnimationPath::Rotation)
    {
        const auto &v=channel.values;
        const auto a=Quaternion{v[first],v[first+1],v[first+2],v[first+3]}.normalized();
        const auto b=Quaternion{v[second],v[second+1],v[second+2],v[second+3]}.normalized();
        if((a.x==b.x&&a.y==b.y&&a.z==b.z&&a.w==b.w)||
           (a.x==-b.x&&a.y==-b.y&&a.z==-b.z&&a.w==-b.w))return false;
        return true;
    }
    for(std::size_t i=0;i<channel.components;++i)
        if(channel.values[first+i]!=channel.values[second+i])return true;
    return false;
}
// A cubic scale can touch or cross zero between all finite pose probes.
// Inspect its polynomial extrema rather than trusting the sampling grid.
void requireNonSingularScale(const GlbAnimationChannel &channel)
{
    if(channel.path!=GlbAnimationPath::Scale)return;
    const bool cubic=channel.interpolation==GlbAnimationInterpolation::CubicSpline;
    const auto stride=cubic?3u:1u;
    const auto value=[&](std::size_t key,std::size_t component)
    {return channel.values[(key*stride+(cubic?1u:0u))*3+component];};
    for(std::size_t axis=0;axis<3;++axis)
    {
        for(std::size_t key=0;key<channel.seconds.size();++key)
            if(value(key,axis)==0)fail("animated scale has a singular key");
        if(channel.interpolation==GlbAnimationInterpolation::Step)continue;
        for(std::size_t key=1;key<channel.seconds.size();++key)
        {
            const double first=value(key-1,axis),last=value(key,axis);
            if(std::signbit(first)!=std::signbit(last))fail("animated scale crosses zero");
            if(!cubic)continue;
            const long double dt=static_cast<long double>(channel.seconds[key])-channel.seconds[key-1];
            long double v0=first,v1=last;
            long double m0=static_cast<long double>(channel.values[((key-1)*3+2)*3+axis])*dt;
            long double m1=static_cast<long double>(channel.values[(key*3)*3+axis])*dt;
            if(!std::isfinite(m0)||!std::isfinite(m1))fail("cubic scale tangent overflows");
            const auto norm=std::max({std::abs(v0),std::abs(v1),std::abs(m0),std::abs(m1)});
            if(!(norm>0)||!std::isfinite(norm))fail("invalid cubic scale range");
            v0/=norm;v1/=norm;m0/=norm;m1/=norm;
            const long double a=2*v0-2*v1+m0+m1,b=-3*v0+3*v1-2*m0-m1,c=m0,d=v0;
            const long double tolerance=2048*std::numeric_limits<double>::epsilon()*(1+std::abs(a)+std::abs(b)+std::abs(c)+std::abs(d));
            const auto check=[&](long double u)
            {
                if(!(u>0&&u<1))return;
                const auto v=((a*u+b)*u+c)*u+d;
                if(!std::isfinite(v)||std::abs(v)<=tolerance||std::signbit(v)!=std::signbit(v0))
                    fail("cubic scale crosses or approaches zero between keys");
            };
            long double A=3*a,B=2*b,C=c;
            const auto derivativeNorm=std::max({std::abs(A),std::abs(B),std::abs(C)});
            if(derivativeNorm==0)continue;
            A/=derivativeNorm;B/=derivativeNorm;C/=derivativeNorm;
            if(A==0){if(B!=0)check(-C/B);continue;}
            auto discriminant=std::fma(-4*A,C,B*B);
            constexpr auto roundoff=128*std::numeric_limits<double>::epsilon();
            if(discriminant<-roundoff)continue;
            discriminant=std::max(0.L,discriminant);
            const auto q=-.5L*(B+std::copysign(std::sqrt(discriminant),B));
            if(q==0)check(-B/(2*A));
            else{check(q/A);check(C/q);}
        }
    }
}
} // namespace

GlbBakedAnimation bakeGlbAnimationClip(const Object3DHandle &object,std::span<const GlbAnimationNodeBinding> nodes,
    const GlbAnimationSource &source,const GlbAnimationBakeOptions &options)
{
    validate(options);const auto &asset=object.get();
    const double duration=source.duration()>0?source.duration():options.staticDuration;
    const auto boneCount=asset.skeleton()->bones().size(),trackCount=asset.hasRig()?boneCount:0;
    std::size_t sourceValues=0,morphCount=0,sourceKeys=0;
    for(const auto &channel:source.channels())
    {
        if(channel.components>options.maximumChannelValueVisits-sourceValues||
           channel.seconds.size()>options.maximumSourceKeyVisits-sourceKeys)
            throw std::length_error("GLB animation bake source budget exceeded");
        sourceValues+=channel.components;sourceKeys+=channel.seconds.size();
        requireNonSingularScale(channel);
        if(channel.path==GlbAnimationPath::Weights)
        {
            if(channel.components>options.maximumCachedValues-morphCount)
                throw std::length_error("GLB animation bake morph budget exceeded");
            morphCount+=channel.components;
        }
        else if(!asset.hasRig())fail("static mesh node motion cannot be represented by a skeletal clip");
    }
    if(boneCount>options.maximumCachedValues-morphCount)throw std::length_error("GLB animation bake cache budget exceeded");
    const auto cachedValues=boneCount+morphCount,outputDimensions=trackCount+morphCount;
    if(!outputDimensions)fail("animation has no representable skeletal or morph output");
    std::map<double,GlbAnimationPoseSample> cache;
    auto evaluate=[&](double time)->const GlbAnimationPoseSample&
    {
        if(const auto found=cache.find(time);found!=cache.end())return found->second;
        const auto count=cache.size()+1;
        if(count>options.maximumEvaluations||count>options.maximumCachedValues/cachedValues||
           (!nodes.empty()&&count>options.maximumNodeVisits/nodes.size())||
           (sourceValues&&count>options.maximumChannelValueVisits/sourceValues))
            throw std::length_error("GLB animation bake evaluation budget exceeded");
        auto frame=sampleGlbAnimationPose(object,nodes,source,time,AnimationWrap::Clamp,options.maximumMatrixError);
        if(frame.boneLocals.size()!=boneCount||frame.morphs.size()!=morphCount)fail("sample dimensions changed");
        if(!cache.empty())
        {
            const auto &expected=cache.begin()->second.morphs;
            for(std::size_t i=0;i<morphCount;++i)
                if(frame.morphs[i].mesh!=expected[i].mesh||frame.morphs[i].target!=expected[i].target)
                    fail("sample morph identities changed");
        }
        return cache.emplace(time,std::move(frame)).first->second;
    };
    const auto &initial=evaluate(0);
    std::unordered_map<std::size_t,std::size_t> nodeIndex;nodeIndex.reserve(nodes.size());
    std::vector<bool> containsBone(nodes.size(),false);
    for(std::size_t i=0;i<nodes.size();++i)
    {nodeIndex.emplace(nodes[i].sourceNode,i);containsBone[i]=nodes[i].bone!=NoBone;}
    for(std::size_t i=nodes.size();i-->0;)
        if(containsBone[i]&&nodes[i].parentSourceNode!=NoBone)
            containsBone[nodeIndex.at(nodes[i].parentSourceNode)]=true;
    for(const auto &channel:source.channels())if(channel.path!=GlbAnimationPath::Weights)
    {
        const auto found=nodeIndex.find(channel.sourceNode);
        if(found==nodeIndex.end()||!containsBone[found->second])
            fail("node animation cannot be represented by the imported skeleton");
    }
    std::set<double> times;addTime(times,0,options);addTime(times,duration,options);
    std::set<double> discontinuities;
    for(const auto &channel:source.channels())
    {
        for(const auto time:channel.seconds)addTime(times,time,options);
        if(channel.interpolation==GlbAnimationInterpolation::Step)
            for(std::size_t key=1;key<channel.seconds.size();++key)
                if(stepChanged(channel,key))discontinuities.insert(channel.seconds[key]);
    }
    if(!discontinuities.empty()&&options.steps==GlbStepConversion::Reject)
        fail("STEP discontinuity requires an explicit approximation policy");
    const std::set<double> originalTimes=times;
    std::map<double,double> stepStarts;
    std::vector<GlbBakeStepTransition> transitions;
    for(const auto at:discontinuities)
    {
        const auto before=beforeStep(at,options);const auto current=originalTimes.find(at);
        if(current==originalTimes.begin()||before<=*std::prev(current))
            fail("STEP transition overlaps another source key");
        addTime(times,before,options);stepStarts.emplace(at,before);transitions.push_back({before,at});
    }
    const std::vector<double> boundaries(times.begin(),times.end());
    for(std::size_t i=1;i<boundaries.size();++i)
    {
        const auto left=boundaries[i-1],right=boundaries[i];
        const double count=std::ceil((right-left)/options.maximumSampleInterval);
        if(!std::isfinite(count)||count>static_cast<double>(options.maximumSampleTimes-times.size()+1))
            throw std::length_error("GLB animation bake sample budget exceeded");
        const auto segments=static_cast<std::size_t>(std::max(1.,count));double previous=left;
        for(std::size_t step=1;step<segments;++step)
        {
            const auto time=std::lerp(left,right,static_cast<double>(step)/static_cast<double>(segments));
            if(!(time>previous&&time<right))fail("sample times are not representable");
            addTime(times,time,options);previous=time;
        }
    }
    auto checkOutput=[&]()
    {if(times.size()>options.maximumOutputKeys/outputDimensions)throw std::length_error("GLB animation bake output key budget exceeded");};
    checkOutput();std::vector<Interval> pending;pending.reserve(times.size()-1);
    auto previous=times.begin();
    for(auto current=std::next(previous);current!=times.end();++current,++previous)
        pending.push_back({*previous,*current,0});
    GlbBakeErrors observed;
    while(!pending.empty())
    {
        const auto interval=pending.back();pending.pop_back();
        if(const auto step=stepStarts.find(interval.last);step!=stepStarts.end()&&step->second==interval.first)continue;
        const auto &left=evaluate(interval.first),&right=evaluate(interval.last);
        const auto middle=std::midpoint(interval.first,interval.last);
        if(!(middle>interval.first&&middle<interval.last))continue;
        GlbBakeErrors error;
        for(const auto time:{std::midpoint(interval.first,middle),middle,std::midpoint(middle,interval.last)})
        {
            const auto &actual=evaluate(time);const double fraction=(time-interval.first)/(interval.last-interval.first);
            for(std::size_t bone=0;bone<trackCount;++bone)
            {
                const auto predicted=character_detail::blend(left.boneLocals[bone],right.boneLocals[bone],fraction);
                error.position=std::max(error.position,distance(predicted.position,actual.boneLocals[bone].position));
                error.rotation=std::max(error.rotation,rotationError(predicted.rotation,actual.boneLocals[bone].rotation));
                error.scale=std::max(error.scale,distance(predicted.scale,actual.boneLocals[bone].scale));
            }
            for(std::size_t morph=0;morph<morphCount;++morph)
                error.morph=std::max(error.morph,std::abs(actual.morphs[morph].value-
                    std::lerp(left.morphs[morph].value,right.morphs[morph].value,fraction)));
        }
        if(within(error,options)){maximum(observed,error);continue;}
        if(interval.depth>=options.maximumSubdivisionDepth)
            throw std::length_error("GLB animation bake subdivision limit exceeded");
        addTime(times,middle,options);checkOutput();
        pending.push_back({interval.first,middle,interval.depth+1});pending.push_back({middle,interval.last,interval.depth+1});
    }
    std::vector<BoneTrack> tracks;tracks.reserve(trackCount);
    for(std::size_t bone=0;bone<trackCount;++bone){tracks.push_back({bone,{}});tracks.back().keys.reserve(times.size());}
    std::vector<AnimationCurve> curves;std::vector<AnimationMorphBinding> bindings;
    curves.reserve(morphCount);bindings.reserve(morphCount);std::set<std::pair<std::size_t,std::size_t>> seen;
    for(std::size_t morph=0;morph<morphCount;++morph)
    {
        const auto &value=initial.morphs[morph];
        if(!seen.emplace(value.mesh,value.target).second)fail("duplicate morph target identity");
        curves.push_back({"glb/mesh/"+std::to_string(value.mesh)+"/target/"+std::to_string(value.target),{}});
        curves.back().keys.reserve(times.size());bindings.push_back({morph,value.mesh,value.target});
    }
    for(const auto time:times)
    {
        const auto &frame=evaluate(time);
        for(std::size_t bone=0;bone<trackCount;++bone)tracks[bone].keys.push_back({time,frame.boneLocals[bone]});
        for(std::size_t morph=0;morph<morphCount;++morph)curves[morph].keys.push_back({time,frame.morphs[morph].value});
    }
    AnimationClip clip(asset.skeleton(),duration,std::move(tracks),std::move(curves));
    return {object,std::move(clip),std::move(bindings),std::move(transitions),times.size(),cache.size(),observed};
}

GlbAnimationPoseSample sampleGlbBakedAnimation(const GlbBakedAnimation &baked,double seconds,AnimationWrap wrap)
{
    if(wrap!=AnimationWrap::Clamp&&wrap!=AnimationWrap::Loop)fail("invalid animation wrap mode");
    const auto &asset=baked.object.get();
    if(asset.skeleton()!=baked.clip.skeleton()||(!asset.hasRig()&&!baked.clip.tracks().empty()))
        fail("baked clip and asset skeleton mismatch");
    std::set<std::pair<std::size_t,std::size_t>> targets;
    for(const auto &binding:baked.morphBindings)
    {
        if(binding.curve>=baked.clip.curves().size()||binding.mesh>=asset.data().meshes.size()||
           binding.target>=asset.data().meshes[binding.mesh].geometry.morphTargets.size()||
           !targets.emplace(binding.mesh,binding.target).second)
            fail("invalid baked morph binding");
    }
    Pose pose(asset.skeleton());baked.clip.sample(seconds,pose,wrap);
    GlbAnimationPoseSample result;result.object=baked.object;
    result.boneLocals.assign(pose.locals().begin(),pose.locals().end());
    result.morphs.reserve(baked.morphBindings.size());
    for(const auto &binding:baked.morphBindings)
        result.morphs.push_back({binding.mesh,binding.target,baked.clip.sampleCurve(binding.curve,seconds,wrap)});
    return result;
}
} // namespace iiActorObject
