#include "Object3DInterchange.h"
#include "Object3DFbxTangents.h"
#include "Object3DGlbPointIdentity.h"
#include "Object3DGlbTangentPlan.h"
#include "Object3DNames.h"
#include "Animation.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <set>
#include <sstream>

namespace iiActorObject
{
namespace
{
std::string quote(std::string_view text)
{
    constexpr char hex[] = "0123456789abcdef";
    std::string out = "\"";
    for (const unsigned char c : text)
    {
        if (c == '"' || c == '\\')
        {
            out += '\\';
            out += static_cast<char>(c);
        }
        else if (c < 32)
        {
            out += "\\u00";
            out += hex[c >> 4];
            out += hex[c & 15];
        }
        else
            out += static_cast<char>(c);
    }
    out += '"';
    return out;
}
std::ostringstream stream()
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17);
    return out;
}
void u32(std::vector<std::uint8_t> &out, std::uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}
void u16(std::vector<std::uint8_t> &out, std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
}
float narrow(double v)
{
    if (!std::isfinite(v) || std::abs(v) > std::numeric_limits<float>::max())
        throw std::overflow_error("glTF float range exceeded");
    const auto value = static_cast<float>(v);
    if (!std::isfinite(value))
        throw std::overflow_error("glTF float range exceeded");
    return value;
}
void f32(std::vector<std::uint8_t> &out, double value)
{
    u32(out, std::bit_cast<std::uint32_t>(narrow(value)));
}
void align(std::vector<std::uint8_t> &out)
{
    while (out.size() % 4)
        out.push_back(0);
}
struct Buffer final
{
    std::vector<std::uint8_t> bytes;
    std::vector<std::string> views, accessors;
    std::size_t view(std::vector<std::uint8_t> data, unsigned target = 0)
    {
        align(bytes);
        auto s = stream();
        s << "{\"buffer\":0,\"byteOffset\":" << bytes.size() << ",\"byteLength\":" << data.size();
        if (target)
            s << ",\"target\":" << target;
        s << '}';
        views.push_back(s.str());
        bytes.insert(bytes.end(), data.begin(), data.end());
        return views.size() - 1;
    }
    std::size_t accessor(std::size_t viewIndex, unsigned component, std::size_t count,
                         std::string_view type, std::string extra = {})
    {
        auto s = stream();
        s << "{\"bufferView\":" << viewIndex << ",\"componentType\":" << component << ",\"count\":" << count
          << ",\"type\":" << quote(type) << extra << '}';
        accessors.push_back(s.str());
        return accessors.size() - 1;
    }
    std::size_t vectors(std::span<const Vector3> values, bool bounds = false)
    {
        std::vector<std::uint8_t> data;
        std::array<float, 3> low{}, high{};
        bool first = true;
        for (auto p : values)
        {
            const std::array<float, 3> v{narrow(p.x), narrow(p.y), narrow(p.z)};
            for (unsigned i = 0; i < 3; ++i)
            {
                if (first)
                    low[i] = high[i] = v[i];
                else
                {
                    low[i] = std::min(low[i], v[i]);
                    high[i] = std::max(high[i], v[i]);
                }
                f32(data, v[i]);
            }
            first = false;
        }
        std::string extra;
        if (bounds)
        {
            auto s = stream();
            s << ",\"min\":[" << low[0] << ',' << low[1] << ',' << low[2] << "],\"max\":[" << high[0] << ','
              << high[1] << ',' << high[2] << ']';
            extra = s.str();
        }
        return accessor(view(std::move(data), 34962), 5126, values.size(), "VEC3", extra);
    }
    std::size_t sparse(const MorphTarget &morph, std::size_t count, bool normal)
    {
        std::vector<MorphDelta> deltas = morph.deltas;
        std::sort(deltas.begin(), deltas.end(),
                  [](const auto &a, const auto &b) { return a.vertex < b.vertex; });
        std::vector<std::uint8_t> indices, values;
        std::array<float, 3> low{}, high{}; // Include implicit zero deltas.
        bool first = deltas.size() == count;
        for (const auto &delta : deltas)
        {
            const auto p = normal ? delta.normal : delta.position;
            u32(indices, static_cast<std::uint32_t>(delta.vertex));
            const std::array<float, 3> v{narrow(p.x), narrow(p.y), narrow(p.z)};
            for (unsigned i = 0; i < 3; ++i)
            {
                f32(values, v[i]);
                if (first)
                    low[i] = high[i] = v[i];
                else
                {
                    low[i] = std::min(low[i], v[i]);
                    high[i] = std::max(high[i], v[i]);
                }
            }
            first = false;
        }
        auto s = stream();
        s << "{\"componentType\":5126,\"count\":" << count << ",\"type\":\"VEC3\"";
        if (!normal)
            s << ",\"min\":[" << low[0] << ',' << low[1] << ',' << low[2] << "],\"max\":[" << high[0] << ','
              << high[1] << ',' << high[2] << ']';
        if (!deltas.empty())
        {
            const auto iv = view(std::move(indices)), vv = view(std::move(values));
            s << ",\"sparse\":{\"count\":" << deltas.size() << ",\"indices\":{\"bufferView\":" << iv
              << ",\"componentType\":5125},\"values\":{\"bufferView\":" << vv << "}}";
        }
        s << '}';
        accessors.push_back(s.str());
        return accessors.size() - 1;
    }
};
template<class VertexIndex>
void pointIdentityAttribute(Buffer &buffer,const Object3DMesh &mesh,std::size_t count,
                            VertexIndex vertexIndex,std::ostream &attributes)
{
    if(!mesh.topology)return;
    const auto &definition=mesh.topology->layout->definition();std::vector<std::uint8_t> ids;
    if(count>std::numeric_limits<std::size_t>::max()/4)throw std::length_error("GLB point identity attribute size");
    ids.reserve(count*4);
    for(std::size_t i=0;i<count;++i)
    {
        const auto layout=definition.renderToLayout.at(vertexIndex(i));
        const auto point=definition.layouts.at(layout).position;
        u16(ids,static_cast<std::uint16_t>(point));u16(ids,static_cast<std::uint16_t>(point>>16));
    }
    const auto accessor=buffer.accessor(buffer.view(std::move(ids),34962),5123,count,"VEC2");
    attributes<<",\"_IIACTOROBJECT_POINT_ID\":"<<accessor;
}
void tangentExtras(std::ostream &out,std::string_view fallback,std::string_view authoredStatus,
    std::span<const std::string_view> fbxNotes={},std::optional<std::size_t> fbxUv={})
{
    if(fallback.empty()&&authoredStatus.empty()&&fbxNotes.empty()&&!fbxUv)return;
    out<<",\"extras\":{";bool first=true;
    const auto field=[&](std::string_view name)
    {if(!first)out<<',';first=false;out<<quote(name)<<':';};
    if(!fallback.empty()){field("iiActorObjectTangentFallback");out<<quote(fallback);}
    if(!authoredStatus.empty()){field("iiActorObjectAuthoredTangentStatus");out<<quote(authoredStatus);}
    if(!fbxNotes.empty())
    {
        field("iiActorObjectFbxTangentNotes");out<<'[';
        for(std::size_t i=0;i<fbxNotes.size();++i){if(i)out<<',';out<<quote(fbxNotes[i]);}
        out<<']';
    }
    if(fbxUv){field("iiActorObjectFbxTangentUvSet");out<<*fbxUv;}
    out<<'}';
}
std::string tangentPrimitive(Buffer &buffer,const Object3DMesh &mesh,
    const object3d_detail::GlbTangentPlan &plan,bool hasRig,std::size_t influenceSets,std::size_t material,
    std::string_view authoredStatus={},std::span<const std::string_view> fbxNotes={},
    std::optional<std::size_t> fbxUv={})
{
    const auto &geometry=mesh.geometry;const auto count=plan.sourceVertices.size();
    std::vector<Vector3> positions,normals;positions.reserve(count);normals.reserve(count);
    for(const auto source:plan.sourceVertices)
    {
        positions.push_back(geometry.vertices[source].position);
        normals.push_back(geometry.vertices[source].normal);
    }
    const auto position=buffer.vectors(positions,true),normal=buffer.vectors(normals);
    std::vector<std::uint8_t> tangents;
    for(const auto &t:plan.tangents)
    {f32(tangents,t.direction.x);f32(tangents,t.direction.y);f32(tangents,t.direction.z);f32(tangents,t.handedness);}
    const auto tangent=buffer.accessor(buffer.view(std::move(tangents),34962),5126,count,"VEC4");
    auto attributes=stream();attributes<<"{\"POSITION\":"<<position<<",\"NORMAL\":"<<normal<<",\"TANGENT\":"<<tangent;
    for(std::size_t set=0;hasRig&&set<influenceSets;++set)
    {
        std::vector<std::uint8_t> joints,weights;
        for(const auto source:plan.sourceVertices)
        {
            const auto &v=geometry.vertices[source];
            for(std::size_t k=set*4;k<set*4+4;++k)
            {
                u16(joints,static_cast<std::uint16_t>(k<v.weights.size()?v.weights[k].bone:0));
                f32(weights,k<v.weights.size()?v.weights[k].weight:0);
            }
        }
        const auto j=buffer.accessor(buffer.view(std::move(joints),34962),5123,count,"VEC4");
        const auto w=buffer.accessor(buffer.view(std::move(weights),34962),5126,count,"VEC4");
        attributes<<",\"JOINTS_"<<set<<"\":"<<j<<",\"WEIGHTS_"<<set<<"\":"<<w;
    }
    const auto appendUv=[&](std::span<const TextureCoordinate> uv,std::size_t index)
    {
        std::vector<std::uint8_t> values;
        for(const auto source:plan.sourceVertices){f32(values,uv[source].u);f32(values,1-uv[source].v);}
        const auto a=buffer.accessor(buffer.view(std::move(values),34962),5126,count,"VEC2");
        attributes<<",\"TEXCOORD_"<<index<<"\":"<<a;
    };
    if(!mesh.textureCoordinates.empty())appendUv(mesh.textureCoordinates,0);
    for(std::size_t i=0;i<mesh.additionalUvSets.size();++i)appendUv(mesh.additionalUvSets[i].coordinates,i+1);
    pointIdentityAttribute(buffer,mesh,count,[&](std::size_t i){return plan.sourceVertices[i];},attributes);
    attributes<<'}';
    auto targets=stream();
    if(!geometry.morphTargets.empty())
    {
        targets<<",\"targets\":[";
        std::vector<const MorphDelta*> byVertex(geometry.vertices.size());
        for(std::size_t i=0;i<geometry.morphTargets.size();++i)
        {
            if(i)targets<<',';
            const auto &source=geometry.morphTargets[i];
            for(const auto &delta:source.deltas)byVertex[delta.vertex]=&delta;
            MorphTarget mapped;
            for(std::size_t j=0;j<count;++j)
                if(const auto *delta=byVertex[plan.sourceVertices[j]])
                    mapped.deltas.push_back({j,delta->position,delta->normal});
            for(const auto &delta:source.deltas)byVertex[delta.vertex]=nullptr;
            const auto p=buffer.sparse(mapped,count,false),n=buffer.sparse(mapped,count,true);
            targets<<"{\"POSITION\":"<<p<<",\"NORMAL\":"<<n;
            if(!plan.morphTangents[i].deltas.empty())
                targets<<",\"TANGENT\":"<<buffer.sparse(plan.morphTangents[i],count,true);
            targets<<'}';
        }
        targets<<']';
    }
    std::vector<std::uint8_t> indices;for(std::size_t i=0;i<count;++i)u32(indices,static_cast<std::uint32_t>(i));
    const auto accessor=buffer.accessor(buffer.view(std::move(indices),34963),5125,count,"SCALAR");
    auto out=stream();out<<"{\"attributes\":"<<attributes.str()<<",\"indices\":"<<accessor
                         <<",\"material\":"<<material<<targets.str();
    tangentExtras(out,{},authoredStatus,fbxNotes,fbxUv);
    out<<'}';
    return out.str();
}
void strings(std::ostream &out, const std::vector<std::string> &values)
{
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (i)
            out << ',';
        out << values[i];
    }
    out << ']';
}
std::size_t checkAnimation(const Object3DAsset &asset, const AnimationClip &clip,
                           const GlbAnimationExportOptions &options,bool morphAnimation)
{
    if (asset.skeleton().get() != clip.skeleton().get() ||
        (!asset.hasRig() && (!morphAnimation || !clip.tracks().empty())))
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
struct MorphAnimationPlan final
{
    std::size_t mesh=0;
    std::vector<double> times;
    std::vector<std::size_t> curves;
    std::vector<std::uint8_t> values;
};
std::vector<MorphAnimationPlan> planMorphAnimation(const Object3DAsset &asset,const AnimationClip &clip,
    const GlbAnimationExportOptions &options,const GlbMorphAnimationOptions &morphOptions,std::size_t usedKeys)
{
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
    for(const auto &mesh:asset.data().meshes)countEntries(mesh.geometry.morphTargets.size());
    std::map<std::string_view,std::size_t,std::less<>> curves;
    for(std::size_t i=0;i<clip.curves().size();++i)curves.emplace(clip.curves()[i].name,i);
    using Target=std::pair<std::size_t,std::size_t>;
    std::map<std::string_view,std::vector<Target>,std::less<>> morphs;
    for(std::size_t mesh=0;mesh<asset.data().meshes.size();++mesh)
        for(std::size_t target=0;target<asset.data().meshes[mesh].geometry.morphTargets.size();++target)
            morphs[asset.data().meshes[mesh].geometry.morphTargets[target].name].push_back({mesh,target});
    std::map<Target,std::size_t> mapping;std::vector<bool> used(clip.curves().size(),false);
    for(const auto &binding:morphOptions.indexedBindings)
    {
        if(binding.curve>=clip.curves().size()||binding.mesh>=asset.data().meshes.size()||
           binding.target>=asset.data().meshes[binding.mesh].geometry.morphTargets.size())
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
            if(!binding.mesh.empty()&&asset.data().meshes[target.first].name!=binding.mesh)continue;
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
    for(std::size_t meshIndex=0;meshIndex<asset.data().meshes.size();++meshIndex)
    {
        const auto &mesh=asset.data().meshes[meshIndex];
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
std::string animationJson(Buffer &buffer, const AnimationClip &clip,const Object3DAsset &asset,
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
        const auto boneCount=asset.hasRig()?asset.data().bones.size():0;
        auto channel=stream();channel<<"{\"sampler\":"<<index<<",\"target\":{\"node\":"<<boneCount+1+morph.mesh<<",\"path\":\"weights\"}}";
        channels.push_back(channel.str());
    }
    auto out=stream();out<<"{\"samplers\":";strings(out,samplers);out<<",\"channels\":";strings(out,channels);out<<'}';
    return out.str();
}
} // namespace
static std::vector<std::uint8_t> exportGlb(const Object3DAsset &asset, const AnimationClip *clip,
                                           const GlbAnimationExportOptions &options,
                                           const GlbMorphAnimationOptions *morphOptions,
                                           const GlbExportOptions *exportOptions)
{
    const bool external=exportOptions&&!exportOptions->externalDocument.empty();
    if(external)
    {
        if(exportOptions->externalDocument.size()>128)throw std::invalid_argument("GLB external document token exceeds its limit");
        object3d_detail::validateName(exportOptions->externalDocument);
        for(const auto &mesh:asset.data().meshes)
            if(!mesh.topology)throw std::invalid_argument("GLB external edits require source topology");
    }
    if(asset.data().referencePose)throw std::invalid_argument("Bake or clear the reference pose before GLB export");
    for(const auto &mesh:asset.data().meshes)
        if(mesh.geometry.skinning.method!=SkinningMethod::Linear)
            throw std::invalid_argument("GLB export does not preserve dual quaternion skinning");
    std::vector<MorphAnimationPlan> morphs;
    if(clip)
    {
        const auto used=checkAnimation(asset,*clip,options,morphOptions!=nullptr);
        if(morphOptions)morphs=planMorphAnimation(asset,*clip,options,*morphOptions,used);
    }
    const auto &d = asset.data();
    Buffer b;
    const auto boneNodeCount = asset.hasRig() ? d.bones.size() : 0;
    if (d.bones.size() > 65535)
        throw std::invalid_argument("glTF joint index exceeds 16 bits");
    std::vector<std::uint8_t> matrices;
    std::size_t ibm = 0;
    if (asset.hasRig())
    {
        for (const auto &m : asset.skeleton()->inverseBindMatrices())
            for (double value : m.elements())
                f32(matrices, value);
        ibm = b.accessor(b.view(std::move(matrices)), 5126, d.bones.size(), "MAT4");
    }
    std::vector<std::string> meshes;
    // One material can bind each texture to a different UV set, whose index may
    // differ between meshes even when its authoring name is the same.
    using MaterialVariant=std::array<std::size_t,3>;
    std::vector<MaterialVariant> exportedMaterials;
    std::map<MaterialVariant,std::size_t> materialVariants;
    for(std::size_t i=0;i<d.materials.size();++i)
    {
        materialVariants[{i,0,0}]=exportedMaterials.size();exportedMaterials.push_back({i,0,0});
    }
    const auto uvIndex=[](const Object3DMesh &mesh,const std::string &name)
    {
        if(name.empty() || name==(mesh.primaryUvName.empty()?"UVMap":mesh.primaryUvName))return std::size_t{0};
        const auto found=std::find_if(mesh.additionalUvSets.begin(),mesh.additionalUvSets.end(),
            [&](const auto &uv){return uv.name==name;});
        if(found==mesh.additionalUvSets.end())throw std::invalid_argument("GLB material UV set is absent");
        return static_cast<std::size_t>(found-mesh.additionalUvSets.begin())+1;
    };
    const auto materialFor=[&](const Object3DMesh &mesh,std::size_t material)
    {
        const auto &m=d.materials.at(material);
        const MaterialVariant key{material,uvIndex(mesh,m.baseColorUvSet),uvIndex(mesh,m.normalUvSet)};
        const auto [it,added]=materialVariants.emplace(key,exportedMaterials.size());
        if(added)exportedMaterials.push_back(key);
        return it->second;
    };
    for (const auto &mesh : d.meshes)
    {
        const auto sourceMesh=meshes.size();
        const auto &g = mesh.geometry;
        if (g.vertices.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::overflow_error("glTF vertex limit");
        std::size_t influenceSets = 1;
        for (const auto &v : g.vertices)
            influenceSets = std::max(influenceSets, (v.weights.size() + 3) / 4);
        struct TrianglePart {std::vector<std::uint32_t> vertices,corners;std::string_view authoredStatus;};
        std::map<std::pair<std::size_t,bool>,TrianglePart> materialTriangles;
        const auto *topology=mesh.topology?&*mesh.topology:nullptr;
        const bool hasAuthored=topology&&topology->sourceGlbTangents;
        const bool authoredValid=hasAuthored&&topologyGlbTangentsValid(*topology);
        const bool hasFbxSource=topology&&topology->sourceTangents;
        const bool fbxValid=hasFbxSource&&topologySourceTangentsValid(*topology);
        std::vector<std::uint32_t> tangentByLayout;
        if(authoredValid)
        {
            tangentByLayout.assign(topology->layout->definition().layouts.size(),NoTopologyIndex);
            const auto &tangents=topology->sourceGlbTangents->tangents;
            for(std::size_t i=0;i<tangents.size();++i)tangentByLayout.at(tangents[i].layout)=static_cast<std::uint32_t>(i);
        }
        for(std::size_t i=0;i<g.triangles.size();i+=3)
        {
            if(i>std::numeric_limits<std::uint32_t>::max()-2)throw std::overflow_error("glTF corner limit");
            const auto material=mesh.triangleMaterials.empty()?mesh.material:mesh.triangleMaterials[i/3];
            bool useAuthored=false;std::string_view status;
            if(hasAuthored&&!authoredValid)status="source_tangents_stale";
            if(authoredValid)
            {
                unsigned present=0;const auto &definition=topology->layout->definition();
                for(std::size_t corner=0;corner<3;++corner)
                {
                    const auto layout=definition.renderToLayout.at(g.triangles[i+corner]);
                    present+=tangentByLayout.at(layout)!=NoTopologyIndex;
                }
                if(present!=0&&present!=3)throw std::invalid_argument("Incomplete authored GLB tangent triangle");
                if(present==3)
                {
                    const auto &bindings=topology->sourceGlbTangents->bindings;
                    const auto binding=std::lower_bound(bindings.begin(),bindings.end(),material,
                        [](const auto &entry,std::size_t id){return entry.material<id;});
                    const auto &current=d.materials.at(material);
                    if(binding==bindings.end()||binding->material!=material||binding->normalMapped!=current.normalImage.has_value()||
                       (binding->normalMapped&&binding->normalUvSet!=current.normalUvSet))status="source_tangent_uv_changed";
                    else useAuthored=true;
                }
            }
            auto &part=materialTriangles[{material,useAuthored}];if(!status.empty())part.authoredStatus=status;
            part.vertices.insert(part.vertices.end(),g.triangles.begin()+static_cast<std::ptrdiff_t>(i),g.triangles.begin()+static_cast<std::ptrdiff_t>(i+3));
            for(std::size_t corner=0;corner<3;++corner)part.corners.push_back(static_cast<std::uint32_t>(i+corner));
        }
        std::optional<std::pair<std::string,std::string>> shared;
        const auto sharedData=[&]() -> const std::pair<std::string,std::string>&
        {
        if(shared)return *shared;
        std::vector<Vector3> positions,normals;positions.reserve(g.vertices.size());normals.reserve(g.vertices.size());
        for(const auto &v:g.vertices){positions.push_back(v.position);normals.push_back(v.normal);}
        const auto position=b.vectors(positions,true),normal=b.vectors(normals);
        auto attributes = stream();
        attributes << "{\"POSITION\":" << position << ",\"NORMAL\":" << normal;
        for (std::size_t set = 0; asset.hasRig() && set < influenceSets; ++set)
        {
            std::vector<std::uint8_t> joints, weights;
            for (const auto &v : g.vertices)
                for (std::size_t k = set * 4; k < set * 4 + 4; ++k)
                {
                    u16(joints, static_cast<std::uint16_t>(k < v.weights.size() ? v.weights[k].bone : 0));
                    f32(weights, k < v.weights.size() ? v.weights[k].weight : 0);
                }
            const auto j = b.accessor(b.view(std::move(joints), 34962), 5123, g.vertices.size(), "VEC4");
            const auto w = b.accessor(b.view(std::move(weights), 34962), 5126, g.vertices.size(), "VEC4");
            attributes << ",\"JOINTS_" << set << "\":" << j << ",\"WEIGHTS_" << set << "\":" << w;
        }
        if (!mesh.textureCoordinates.empty())
        {
            std::vector<std::uint8_t> uv;
            for (auto v : mesh.textureCoordinates)
            {
                f32(uv, v.u);
                f32(uv, 1 - v.v);
            }
            const auto a = b.accessor(b.view(std::move(uv), 34962), 5126, g.vertices.size(), "VEC2");
            attributes << ",\"TEXCOORD_0\":" << a;
        }
        for(std::size_t set=0;set<mesh.additionalUvSets.size();++set)
        {
            std::vector<std::uint8_t> uv;
            for(const auto v:mesh.additionalUvSets[set].coordinates)
            {
                f32(uv,v.u);
                f32(uv,1-v.v);
            }
            const auto a=b.accessor(b.view(std::move(uv),34962),5126,g.vertices.size(),"VEC2");
            attributes << ",\"TEXCOORD_" << set+1 << "\":" << a;
        }
        pointIdentityAttribute(b,mesh,g.vertices.size(),[](std::size_t i){return i;},attributes);
        attributes << '}';
        auto targets=stream();
        if (!g.morphTargets.empty())
        {
            targets << ",\"targets\":[";
            for (std::size_t i = 0; i < g.morphTargets.size(); ++i)
            {
                if (i)
                    targets << ',';
                const auto p = b.sparse(g.morphTargets[i], g.vertices.size(), false),
                           n = b.sparse(g.morphTargets[i], g.vertices.size(), true);
                targets << "{\"POSITION\":" << p << ",\"NORMAL\":" << n << '}';
            }
            targets << ']';
        }
        shared.emplace(attributes.str(),targets.str());return *shared;
        };
        auto s=stream();s<<"{\"name\":"<<quote(mesh.name)<<",\"primitives\":[";bool firstPrimitive=true;
        std::size_t authoredWork=0;
        bool fbxProjectionAttempted=false;
        std::optional<object3d_detail::FbxTangentProjection> fbxProjection;
        std::string_view fbxProjectionFailure;
        for(const auto &[key,part]:materialTriangles)
        {
            const auto [material,useAuthored]=key;
            if(!firstPrimitive)s<<',';firstPrimitive=false;
            const auto exportedMaterial=materialFor(mesh,material);const auto &m=d.materials[material];
            if(useAuthored)
            {
                const auto plan=object3d_detail::planAuthoredGlbTangents(mesh,part.corners,tangentByLayout,authoredWork);
                s<<tangentPrimitive(b,mesh,plan,asset.hasRig(),influenceSets,exportedMaterial);
                continue;
            }
            const auto writeFallback=[&](const TrianglePart &fallbackPart,std::span<const std::string_view> notes,
                                         std::optional<std::size_t> unboundUv)
            {
                std::string_view tangentFallback;
                if(m.normalImage)
                {
                    const auto index=uvIndex(mesh,m.normalUvSet);
                    const std::span<const TextureCoordinate> uv=index==0?std::span<const TextureCoordinate>(mesh.textureCoordinates):
                        std::span<const TextureCoordinate>(mesh.additionalUvSets[index-1].coordinates);
                    if(auto plan=object3d_detail::planGlbTangents(mesh,fallbackPart.corners,uv,tangentFallback))
                    {
                        s<<tangentPrimitive(b,mesh,*plan,asset.hasRig(),influenceSets,exportedMaterial,
                                            fallbackPart.authoredStatus,notes,unboundUv);
                        return;
                    }
                }
                const auto &[attributes,targets]=sharedData();
                std::vector<std::uint8_t> triangles;for(const auto index:fallbackPart.vertices)u32(triangles,index);
                const auto indices=b.accessor(b.view(std::move(triangles),34963),5125,fallbackPart.vertices.size(),"SCALAR");
                s<<"{\"attributes\":"<<attributes<<",\"indices\":"<<indices<<",\"material\":"<<exportedMaterial<<targets;
                tangentExtras(s,tangentFallback,fallbackPart.authoredStatus,notes,unboundUv);
                s<<'}';
            };
            std::vector<std::string_view> fbxNotes;
            const TopologyTangentSourceLayer *fbxLayer=nullptr;
            std::optional<std::size_t> unboundUv;
            if(hasFbxSource&&!fbxValid)fbxNotes.push_back("source_fbx_tangents_stale");
            if(fbxValid)
            {
                const auto &layers=topology->sourceTangents->layers;
                if(m.normalImage)
                {
                    const auto selected=uvIndex(mesh,m.normalUvSet);
                    const auto found=std::find_if(layers.begin(),layers.end(),
                        [&](const auto &layer){return layer.uvSet==selected;});
                    if(found==layers.end())fbxNotes.push_back("source_fbx_uv_layer_missing");
                    else if(found->tangents.empty())fbxNotes.push_back("source_fbx_tangent_missing");
                    else fbxLayer=&*found;
                }
                else
                {
                    for(const auto &layer:layers)
                        if(!layer.tangents.empty())
                        {
                            if(fbxLayer){fbxLayer=nullptr;fbxNotes.push_back("source_fbx_uv_ambiguous");break;}
                            fbxLayer=&layer;
                        }
                    if(!fbxLayer&&fbxNotes.empty())fbxNotes.push_back("source_fbx_tangent_missing");
                    if(fbxLayer)fbxNotes.push_back("source_fbx_unbound_uv");
                }
                if(fbxLayer)
                {
                    if(!m.normalImage)unboundUv=fbxLayer->uvSet;
                    if(std::any_of(layers.begin(),layers.end(),[&](const auto &layer){return layer.uvSet!=fbxLayer->uvSet;}))
                        fbxNotes.push_back("source_fbx_other_uv_layers_omitted");
                    if(!fbxProjectionAttempted)
                    {
                        fbxProjection=object3d_detail::projectFbxTangentCorners(mesh,fbxProjectionFailure);
                        fbxProjectionAttempted=true;
                    }
                    if(!fbxProjection)fbxNotes.push_back(fbxProjectionFailure);
                    else
                    {
                        auto split=object3d_detail::planFbxGlbTangents(mesh,*fbxLayer,part.corners,
                                                       fbxProjection->triangleToPolygon,fbxNotes);
                        if(!split.authored.sourceVertices.empty())
                        {
                            s<<tangentPrimitive(b,mesh,split.authored,asset.hasRig(),influenceSets,exportedMaterial,
                                                part.authoredStatus,split.authoredNotes,unboundUv);
                            if(!split.fallbackCorners.empty())
                            {
                                s<<',';TrianglePart remainder;remainder.corners=std::move(split.fallbackCorners);
                                remainder.authoredStatus=part.authoredStatus;remainder.vertices.reserve(remainder.corners.size());
                                for(const auto corner:remainder.corners)remainder.vertices.push_back(g.triangles.at(corner));
                                writeFallback(remainder,split.fallbackNotes,unboundUv);
                            }
                            continue;
                        }
                        fbxNotes=std::move(split.fallbackNotes);
                    }
                }
            }
            writeFallback(part,fbxNotes,unboundUv);
        }
        s << "]";
        if (!g.morphTargets.empty())
        {
            std::map<std::string_view,double,std::less<>> defaults;
            for(const auto &entry:mesh.morphDefaults)defaults.emplace(entry.name,entry.value);
            s << ",\"weights\":[";
            for (std::size_t i = 0; i < g.morphTargets.size(); ++i)
            {
                if (i)
                    s << ',';
                const auto found=defaults.find(g.morphTargets[i].name);
                const double value=found==defaults.end()?0.:found->second;
                const auto encoded=narrow(value);
                if(value!=0&&encoded==0)throw std::invalid_argument("GLB default morph weight underflows float32");
                s << encoded;
            }
            s << ']';
        }
        if(!g.morphTargets.empty()||mesh.topology)
        {
            s<<",\"extras\":{";bool extra=false;
            if(!g.morphTargets.empty())
            {
                s<<"\"targetNames\":[";
            for (std::size_t i = 0; i < g.morphTargets.size(); ++i)
            {
                if (i)
                    s << ',';
                s << quote(g.morphTargets[i].name);
            }
                s<<']';extra=true;
            }
            if(mesh.topology)
            {
                const auto metadata=b.view(object3d_detail::encodeGlbPointIdentity(mesh,
                    asset.hasRig()?std::span<const Bone>(d.bones):std::span<const Bone>{}));
                if(extra)s<<',';
                s<<"\"iiActorObjectPoints\":{\"version\":1,\"bufferView\":"<<metadata<<'}';
                extra=true;
            }
            if(external){if(extra)s<<',';s<<"\"iiActorObjectSourceMesh\":"<<sourceMesh;}
            s<<'}';
        }
        s << '}';
        meshes.push_back(s.str());
    }
    std::vector<std::size_t> imageViews;
    for(const auto &image:d.images)imageViews.push_back(b.view(std::vector<std::uint8_t>(image.encoded().begin(),image.encoded().end())));
    const auto animation=clip?animationJson(b,*clip,asset,morphs):std::string{};
    if(external&&exportedMaterials.size()!=d.materials.size())
        throw std::invalid_argument("GLB external edits cannot identify UV-specific material variants");
    auto json = stream();
    json << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"iiActorObject Object3DEngine\"";
    if(external)json<<",\"extras\":{\"iiActorObjectExternalVersion\":1,\"iiActorObjectExternalDocument\":"
                    <<quote(exportOptions->externalDocument)<<'}';
    json << "},\"scene\":0,\"scenes\":[{\"nodes\":["
         << boneNodeCount << "]}],\"nodes\":[";
    for (std::size_t i = 0; i < boneNodeCount; ++i)
    {
        if (i)
            json << ',';
        const auto &bone = d.bones[i];
        const auto &t = bone.bindTransform;
        json << "{\"name\":" << quote(bone.name) << ",\"translation\":[" << t.position.x << ','
             << t.position.y << ',' << t.position.z << "],\"rotation\":[" << t.rotation.x << ','
             << t.rotation.y << ',' << t.rotation.z << ',' << t.rotation.w << "],\"scale\":[" << t.scale.x
             << ',' << t.scale.y << ',' << t.scale.z << ']';
        if(external)json<<",\"extras\":{\"iiActorObjectSourceBone\":"<<i<<'}';
        bool first = true;
        for (std::size_t c = 0; c < d.bones.size(); ++c)
            if (d.bones[c].parent == i)
            {
                json << (first ? ",\"children\":[" : ",") << c;
                first = false;
            }
        if (!first)
            json << ']';
        json << '}';
    }
    if (boneNodeCount)
        json << ',';
    json << "{\"name\":" << quote(d.name) << ",\"children\":[";
    bool firstChild = true;
    if (boneNodeCount)
    {
        json << '0';
        firstChild = false;
    }
    for (std::size_t i = 0; i < d.meshes.size(); ++i)
    {
        if (!firstChild)
            json << ',';
        json << boneNodeCount + 1 + i;
        firstChild = false;
    }
    json << "],\"extras\":{\"iiObject3D\":{\"schemaVersion\":1,\"semantics\":{";
    for (std::size_t i = 0; i < d.semantics.size(); ++i)
    {
        if (i)
            json << ',';
        json << quote(d.semantics[i].name) << ':' << d.semantics[i].bone;
    }
    json << '}';
    if(!d.rigSampleCatalogs.empty())json << ",\"rigSamples\":" << exportRigSampleCatalogsJson(d.rigSampleCatalogs);
    json << "}}}";
    for (std::size_t i = 0; i < d.meshes.size(); ++i)
    {
        json << ",{\"name\":" << quote(d.meshes[i].name) << ",\"mesh\":" << i;
        if (asset.hasRig())
            json << ",\"skin\":0";
        json << '}';
    }
    json << ']';
    if (asset.hasRig())
    {
        json << ",\"skins\":[{\"inverseBindMatrices\":" << ibm << ",\"skeleton\":0,\"joints\":[";
        for (std::size_t i = 0; i < d.bones.size(); ++i)
        {
            if (i)
                json << ',';
            json << i;
        }
        json << "]}]";
    }
    json << ",\"meshes\":";
    strings(json, meshes);
    json << ",\"materials\":[";
    for (std::size_t i = 0; i < exportedMaterials.size(); ++i)
    {
        if (i)
            json << ',';
        const auto &[source,uvSet,normalUvSet]=exportedMaterials[i];const auto &m = d.materials[source];
        json << "{\"name\":" << quote(m.name) << ",\"pbrMetallicRoughness\":{\"baseColorFactor\":[";
        for (unsigned c = 0; c < 4; ++c)
        {
            if (c)
                json << ',';
            json << m.baseColor[c];
        }
        json << "],\"metallicFactor\":" << m.metallic << ",\"roughnessFactor\":" << m.roughness;
        if(m.baseColorImage)json<<",\"baseColorTexture\":{\"index\":"<<*m.baseColorImage<<",\"texCoord\":"<<uvSet<<'}';
        json<<'}';
        if(m.normalImage)json<<",\"normalTexture\":{\"index\":"<<*m.normalImage<<",\"texCoord\":"<<normalUvSet<<",\"scale\":"<<m.normalScale<<'}';
        const auto mode=effectiveAlphaMode(m);
        if(mode==Object3DAlphaMode::Mask)json<<",\"alphaMode\":\"MASK\"";
        else if(mode==Object3DAlphaMode::Blend)json<<",\"alphaMode\":\"BLEND\"";
        else if(m.surface.alphaMode==Object3DAlphaMode::Opaque)json<<",\"alphaMode\":\"OPAQUE\"";
        if(mode==Object3DAlphaMode::Mask||m.surface.alphaCutoff!=.5)json<<",\"alphaCutoff\":"<<m.surface.alphaCutoff;
        if(m.surface.doubleSided)json<<",\"doubleSided\":true";
        if(external)json<<",\"extras\":{\"iiActorObjectSourceMaterial\":"<<source<<'}';
        json << '}';
    }
    json<<']';
    if(!d.images.empty())
    {
        json<<",\"images\":[";
        for(std::size_t i=0;i<d.images.size();++i){if(i)json<<',';json<<"{\"name\":"<<quote(d.images[i].name())<<",\"mimeType\":"<<quote(d.images[i].mimeType())<<",\"bufferView\":"<<imageViews[i]<<'}';}
        json<<"],\"samplers\":[{\"magFilter\":9729,\"minFilter\":9729,\"wrapS\":10497,\"wrapT\":10497}],\"textures\":[";
        for(std::size_t i=0;i<d.images.size();++i){if(i)json<<',';json<<"{\"source\":"<<i<<",\"sampler\":0}";}json<<']';
    }
    if(clip)json<<",\"animations\":["<<animation<<']';
    json << ",\"bufferViews\":";
    strings(json, b.views);
    json << ",\"accessors\":";
    strings(json, b.accessors);
    json << ",\"buffers\":[{\"byteLength\":" << b.bytes.size() << "}]}";
    auto text = json.str();
    while (text.size() % 4)
        text += ' ';
    align(b.bytes);
    const auto total = 12ull + 8 + text.size() + 8 + b.bytes.size();
    if (total > std::numeric_limits<std::uint32_t>::max())
        throw std::overflow_error("GLB exceeds four GiB");
    std::vector<std::uint8_t> out;
    u32(out, 0x46546c67);
    u32(out, 2);
    u32(out, static_cast<std::uint32_t>(total));
    u32(out, static_cast<std::uint32_t>(text.size()));
    u32(out, 0x4e4f534a);
    out.insert(out.end(), text.begin(), text.end());
    u32(out, static_cast<std::uint32_t>(b.bytes.size()));
    u32(out, 0x004e4942);
    out.insert(out.end(), b.bytes.begin(), b.bytes.end());
    return out;
}
std::vector<std::uint8_t> exportObject3DGlb(const Object3DAsset &asset)
{
    return exportGlb(asset,nullptr,{},nullptr,nullptr);
}
std::vector<std::uint8_t> exportObject3DGlb(const Object3DAsset &asset,const GlbExportOptions &options)
{
    return exportGlb(asset,nullptr,{},nullptr,&options);
}
std::vector<std::uint8_t> exportObject3DGlb(const Object3DAsset &asset, const AnimationClip &clip,
                                             const GlbAnimationExportOptions &options)
{
    return exportGlb(asset,&clip,options,nullptr,nullptr);
}
std::vector<std::uint8_t> exportObject3DGlb(const Object3DAsset &asset,const AnimationClip &clip,
    const GlbAnimationExportOptions &options,const GlbMorphAnimationOptions &morphOptions)
{
    return exportGlb(asset,&clip,options,&morphOptions,nullptr);
}
} // namespace iiActorObject
