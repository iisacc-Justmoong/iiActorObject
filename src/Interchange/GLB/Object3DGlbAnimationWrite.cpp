#include "Object3DGlbAnimationWrite.h"
#include "Object3DGlbBuffer.h"
#include "Object3DData.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail::glb_export
{
std::size_t checkGlbAnimation(const Skeleton *skeleton,bool hasRig,const AnimationClip &clip,
                           const GlbAnimationExportOptions &options,bool morphAnimation)
{
    if (skeleton != clip.skeleton().get() ||
        (!hasRig && (!morphAnimation || !clip.tracks().empty())))
        throw std::invalid_argument("GLB animation requires the asset's own skeleton");
    if(!clip.requiredDeformationBind().empty())
        throw std::invalid_argument("GLB animation requires an asset with the deformation baked into its skeleton");
    if (!morphAnimation && clip.tracks().empty())
        throw std::invalid_argument("GLB animation requires at least one bone track");
    if (!morphAnimation && !options.allowDiscardScalarCurves && !clip.curves().empty())
        throw std::invalid_argument("GLB animation cannot infer scalar curve targets");
    if (options.maximumKeys == 0 || options.maximumKeys > 10'000'000)
        throw std::invalid_argument("Invalid GLB animation key budget");
    std::size_t total = 0;
    for (const auto &track : clip.tracks())
    {
        const auto extra = static_cast<std::size_t>(track.keys.front().seconds > 0) +
                           static_cast<std::size_t>(track.keys.back().seconds < clip.duration());
        if (track.keys.size() > options.maximumKeys - total ||
            extra > options.maximumKeys - total - track.keys.size())
            throw std::length_error("GLB animation key budget");
        total += track.keys.size() + extra;
        float previous = -1;
        const auto checkTime = [&](double time)
        {
            const auto current = narrow(time);
            if (current <= previous)
                throw std::invalid_argument("GLB animation times collide in float32");
            previous = current;
        };
        if (track.keys.front().seconds > 0)
            checkTime(0);
        for (const auto &key : track.keys)
        {
            checkTime(key.seconds);
            const auto &t = key.value;
            (void)narrow(t.position.x); (void)narrow(t.position.y); (void)narrow(t.position.z);
            (void)narrow(t.rotation.x); (void)narrow(t.rotation.y); (void)narrow(t.rotation.z); (void)narrow(t.rotation.w);
            if (narrow(t.scale.x) == 0 || narrow(t.scale.y) == 0 || narrow(t.scale.z) == 0)
                throw std::invalid_argument("GLB animation scale underflows float32");
        }
        if (track.keys.back().seconds < clip.duration())
            checkTime(clip.duration());
    }
    return total;
}
std::vector<MorphAnimationPlan> planGlbMorphAnimation(const Object3DData &data,const AnimationClip &clip,
    const GlbAnimationExportOptions &options,const GlbMorphAnimationOptions &morphOptions,std::size_t usedKeys)
{
    if(!options.maximumKeys||options.maximumKeys>10'000'000||usedKeys>options.maximumKeys)
        throw std::invalid_argument("Invalid GLB animation key budget");
    constexpr std::size_t limit=100'000'000;
    constexpr std::size_t mappingLimit=10'000'000;
    constexpr auto noCurve=std::numeric_limits<std::size_t>::max();
    if(!morphOptions.maximumWeightValues||morphOptions.maximumWeightValues>limit||
       !morphOptions.maximumCurveKeyVisits||morphOptions.maximumCurveKeyVisits>limit||
       !morphOptions.maximumMappingEntries||morphOptions.maximumMappingEntries>mappingLimit)
        throw std::invalid_argument("Invalid GLB morph animation budget");
    std::size_t entries=0;
    const auto countEntries=[&](std::size_t value)
    {
        if(value>morphOptions.maximumMappingEntries-entries)throw std::length_error("GLB morph mapping budget");
        entries+=value;
    };
    countEntries(clip.curves().size());countEntries(morphOptions.bindings.size());countEntries(morphOptions.indexedBindings.size());
    for(const auto &mesh:data.meshes)countEntries(mesh.geometry.morphTargets.size());
    std::map<std::string_view,std::size_t,std::less<>> curves;
    for(std::size_t i=0;i<clip.curves().size();++i)curves.emplace(clip.curves()[i].name,i);
    using Target=std::pair<std::size_t,std::size_t>;
    std::map<std::string_view,std::vector<Target>,std::less<>> morphs;
    for(std::size_t mesh=0;mesh<data.meshes.size();++mesh)
        for(std::size_t target=0;target<data.meshes[mesh].geometry.morphTargets.size();++target)
            morphs[data.meshes[mesh].geometry.morphTargets[target].name].push_back({mesh,target});
    std::map<Target,std::size_t> mapping;std::vector<bool> used(clip.curves().size(),false);
    for(const auto &binding:morphOptions.indexedBindings)
    {
        if(binding.curve>=clip.curves().size()||binding.mesh>=data.meshes.size()||
           binding.target>=data.meshes[binding.mesh].geometry.morphTargets.size())
            throw std::invalid_argument("GLB indexed morph binding is out of range");
        if(!mapping.emplace(Target{binding.mesh,binding.target},binding.curve).second)
            throw std::invalid_argument("GLB morph has more than one binding on the same mesh");
        used[binding.curve]=true;
    }
    for(const auto &binding:morphOptions.bindings)
    {
        const auto curve=curves.find(binding.curve);const auto targets=morphs.find(binding.morph);
        if(curve==curves.end()||targets==morphs.end())
            throw std::invalid_argument("GLB morph binding refers to a missing curve or morph");
        bool matched=false;
        for(const auto target:targets->second)
        {
            if(!binding.mesh.empty()&&data.meshes[target.first].name!=binding.mesh)continue;
            if(!mapping.emplace(target,curve->second).second)
                throw std::invalid_argument("GLB morph has more than one binding on the same mesh");
            matched=true;
        }
        if(!matched)throw std::invalid_argument("GLB morph binding refers to a missing mesh target");
        used[curve->second]=true;
    }
    if(morphOptions.matchUnmappedCurvesByName)
        for(std::size_t i=0;i<clip.curves().size();++i)
        {
            const auto name=std::string_view(clip.curves()[i].name);
            const auto targets=morphs.find(name);
            if(used[i]||targets==morphs.end())continue;
            for(const auto target:targets->second)
                if(!mapping.emplace(target,i).second)throw std::invalid_argument("GLB automatic morph binding conflicts with an explicit binding");
            used[i]=true;
        }
    if(!options.allowDiscardScalarCurves&&std::find(used.begin(),used.end(),false)!=used.end())
        throw std::invalid_argument("GLB scalar curve has no explicitly enabled morph binding");
    std::vector<MorphAnimationPlan> plans;std::size_t visits=0,weights=0;
    for(std::size_t meshIndex=0;meshIndex<data.meshes.size();++meshIndex)
    {
        const auto &mesh=data.meshes[meshIndex];
        MorphAnimationPlan plan;plan.mesh=meshIndex;plan.curves.resize(mesh.geometry.morphTargets.size(),noCurve);
        std::set<std::size_t> sources;
        for(std::size_t target=0;target<mesh.geometry.morphTargets.size();++target)
        {
            const auto found=mapping.find({meshIndex,target});
            if(found!=mapping.end()){plan.curves[target]=found->second;sources.insert(found->second);}
        }
        if(sources.empty())continue;
        plan.times={0,clip.duration()};
        for(const auto index:sources)
        {
            const auto &keys=clip.curves()[index].keys;
            if(keys.size()>morphOptions.maximumCurveKeyVisits-visits)throw std::length_error("GLB morph curve visit budget");
            visits+=keys.size();for(const auto &key:keys)plan.times.push_back(key.seconds);
        }
        std::sort(plan.times.begin(),plan.times.end());
        plan.times.erase(std::unique(plan.times.begin(),plan.times.end()),plan.times.end());
        if(plan.times.size()>options.maximumKeys-usedKeys)throw std::length_error("GLB animation key budget");
        usedKeys+=plan.times.size();
        if(plan.curves.size()>(morphOptions.maximumWeightValues-weights)/plan.times.size())
            throw std::length_error("GLB morph weight value budget");
        weights+=plan.curves.size()*plan.times.size();
        float previous=-1;
        for(const auto time:plan.times)
        {
            const auto current=narrow(time);
            if(current<=previous)throw std::invalid_argument("GLB morph animation times collide in float32");
            previous=current;
        }
        plan.values.reserve(plan.times.size()*plan.curves.size()*4);
        std::map<std::string_view,double,std::less<>> defaults;
        for(const auto &entry:mesh.morphDefaults)defaults.emplace(entry.name,entry.value);
        for(const auto time:plan.times)
            for(std::size_t target=0;target<plan.curves.size();++target)
            {
                const auto curve=plan.curves[target];
                const auto initial=defaults.find(mesh.geometry.morphTargets[target].name);
                const auto value=curve==noCurve?(initial==defaults.end()?0.:initial->second):clip.sampleCurve(curve,time);
                if(value!=0&&narrow(value)==0)throw std::invalid_argument("GLB morph weight underflows float32");
                f32(plan.values,value);
            }
        plans.push_back(std::move(plan));
    }
    if(clip.tracks().empty()&&plans.empty())throw std::invalid_argument("GLB animation has no exported channels");
    return plans;
}
std::string writeGlbAnimation(Buffer &buffer,const AnimationClip &clip,std::size_t boneNodeCount,
                          std::vector<MorphAnimationPlan> &morphs)
{
    std::vector<std::string> samplers, channels;
    for (const auto &track : clip.tracks())
    {
        std::vector<TransformKey> keys;
        keys.reserve(track.keys.size() + 2);
        if (track.keys.front().seconds > 0)
            keys.push_back({0, track.keys.front().value});
        keys.insert(keys.end(), track.keys.begin(), track.keys.end());
        if (track.keys.back().seconds < clip.duration())
            keys.push_back({clip.duration(), track.keys.back().value});
        std::vector<std::uint8_t> times;
        std::array<std::vector<std::uint8_t>,3> values;
        for (const auto &key : keys)
        {
            f32(times,key.seconds);
            const auto &t=key.value;
            for (auto x : std::array{t.position.x,t.position.y,t.position.z}) f32(values[0],x);
            for (auto x : std::array{t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w}) f32(values[1],x);
            for (auto x : std::array{t.scale.x,t.scale.y,t.scale.z}) f32(values[2],x);
        }
        auto bounds=stream();bounds<<",\"min\":["<<narrow(keys.front().seconds)<<"],\"max\":["
                                   <<narrow(keys.back().seconds)<<']';
        const auto input=buffer.accessor(buffer.view(std::move(times)),5126,keys.size(),"SCALAR",bounds.str());
        constexpr std::array paths{std::string_view{"translation"},std::string_view{"rotation"},std::string_view{"scale"}};
        for (std::size_t i=0;i<paths.size();++i)
        {
            const auto output=buffer.accessor(buffer.view(std::move(values[i])),5126,keys.size(),i==1?"VEC4":"VEC3");
            auto sampler=stream();sampler<<"{\"input\":"<<input<<",\"output\":"<<output<<",\"interpolation\":\"LINEAR\"}";
            const auto index=samplers.size();samplers.push_back(sampler.str());
            auto channel=stream();channel<<"{\"sampler\":"<<index<<",\"target\":{\"node\":"<<track.bone<<",\"path\":"<<quote(paths[i])<<"}}";
            channels.push_back(channel.str());
        }
    }
    for(auto &morph:morphs)
    {
        std::vector<std::uint8_t> times;times.reserve(morph.times.size()*4);
        for(const auto time:morph.times)f32(times,time);
        auto bounds=stream();bounds<<",\"min\":["<<narrow(morph.times.front())<<"],\"max\":["<<narrow(morph.times.back())<<']';
        const auto input=buffer.accessor(buffer.view(std::move(times)),5126,morph.times.size(),"SCALAR",bounds.str());
        const auto output=buffer.accessor(buffer.view(std::move(morph.values)),5126,morph.times.size()*morph.curves.size(),"SCALAR");
        auto sampler=stream();sampler<<"{\"input\":"<<input<<",\"output\":"<<output<<",\"interpolation\":\"LINEAR\"}";
        const auto index=samplers.size();samplers.push_back(sampler.str());
        const auto boneCount=boneNodeCount;
        auto channel=stream();channel<<"{\"sampler\":"<<index<<",\"target\":{\"node\":"<<boneCount+1+morph.mesh<<",\"path\":\"weights\"}}";
        channels.push_back(channel.str());
    }
    auto out=stream();out<<"{\"samplers\":";jsonArray(out,samplers);out<<",\"channels\":";jsonArray(out,channels);out<<'}';
    return out.str();
}
}
