#include "GlbAnimationSource.h"
#include "CharacterMath.h"
#include <iiJson/Json.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace iiActorObject
{
GlbImportResult importObject3DGlbMapped(const Object3DReference &,std::span<const std::uint8_t>,
    const GlbImportOptions &,std::vector<GlbAnimationNodeBinding> &);
namespace
{
using Value=ii::json::Value;
using Array=Value::Array;
[[noreturn]] void fail(std::string_view message) {throw std::invalid_argument("GLB animation: "+std::string(message));}
const Value::Object &object(const Value &value)
{const auto result=value.get<Value::Object>();if(!result)fail("expected an object");return result->get();}
const Array &array(const Value &value)
{const auto result=value.get<Array>();if(!result)fail("expected an array");return result->get();}
const Value *field(const Value &value,std::string_view name)
{const auto &fields=object(value);const auto i=fields.find(name);return i==fields.end()?nullptr:&i->second;}
const Value &required(const Value &value,std::string_view name)
{const auto *result=field(value,name);if(!result)fail("missing "+std::string(name));return *result;}
const Array &list(const Value &value,std::string_view name)
{static const Array empty;const auto *result=field(value,name);return result?array(*result):empty;}
std::string_view text(const Value &value)
{const auto result=value.get<std::string>();if(!result)fail("expected a string");return result->get();}
std::size_t integer(const Value &value)
{
    if(const auto *n=std::get_if<std::uint64_t>(&value.storage()))
    {if(*n>std::numeric_limits<std::size_t>::max())fail("integer out of range");return static_cast<std::size_t>(*n);}
    if(const auto *n=std::get_if<std::int64_t>(&value.storage());n&&*n>=0)
    {if(static_cast<std::uint64_t>(*n)>std::numeric_limits<std::size_t>::max())fail("integer out of range");return static_cast<std::size_t>(*n);}
    fail("expected nonnegative integer");
}
void noExtensions(const Value &value)
{if(const auto *v=field(value,"extensions");v&&!object(*v).empty())fail("animation extensions are unsupported");}
void checkBufferViewExtensions(const Value &holder,const Array &views)
{
    if(const auto *v=field(holder,"bufferView"))
    {const auto i=integer(*v);if(i>=views.size())fail("invalid animation buffer view");noExtensions(views[i]);}
}
void checkAccessorExtensions(const Value &root,std::size_t index)
{
    const auto &accessors=list(root,"accessors"),&views=list(root,"bufferViews");
    if(index>=accessors.size())fail("invalid animation accessor");
    const auto &accessor=accessors[index];noExtensions(accessor);checkBufferViewExtensions(accessor,views);
    if(const auto *sparse=field(accessor,"sparse"))
    {
        noExtensions(*sparse);const auto &indices=required(*sparse,"indices"),&values=required(*sparse,"values");
        noExtensions(indices);noExtensions(values);checkBufferViewExtensions(indices,views);checkBufferViewExtensions(values,views);
    }
}
void consume(std::size_t &total,std::size_t amount,std::size_t limit)
{if(total>limit||amount>limit-total)fail("decoded animation value limit");total+=amount;}
std::size_t checkedProduct(std::size_t a,std::size_t b)
{if(b&&a>std::numeric_limits<std::size_t>::max()/b)fail("animation size overflow");return a*b;}
bool normalizedComponent(const GlbAccessorInfo &info)
{
    if(info.component==GlbComponentType::Float)return !info.normalized;
    return info.normalized&&(info.component==GlbComponentType::Byte||info.component==GlbComponentType::UnsignedByte||
        info.component==GlbComponentType::Short||info.component==GlbComponentType::UnsignedShort);
}
std::size_t morphCount(const Value &node,const Array &meshes,std::size_t limit)
{
    const auto *mesh=field(node,"mesh");if(!mesh)fail("weights channel has no mesh");
    const auto index=integer(*mesh);if(index>=meshes.size())fail("invalid animated mesh");
    const auto &primitives=array(required(meshes[index],"primitives"));
    if(primitives.empty())fail("animated mesh has no primitives");
    const auto count=list(primitives.front(),"targets").size();
    if(!count||count>limit)fail("invalid animated morph count");
    for(const auto &primitive:primitives)if(list(primitive,"targets").size()!=count)fail("inconsistent animated morph count");
    return count;
}
GlbAnimationPath path(std::string_view value)
{
    if(value=="translation")return GlbAnimationPath::Translation;
    if(value=="rotation")return GlbAnimationPath::Rotation;
    if(value=="scale")return GlbAnimationPath::Scale;
    if(value=="weights")return GlbAnimationPath::Weights;
    fail("unsupported channel target");
}
GlbAnimationInterpolation interpolation(const Value &sampler)
{
    const auto *v=field(sampler,"interpolation");const auto name=v?text(*v):"LINEAR";
    if(name=="LINEAR")return GlbAnimationInterpolation::Linear;
    if(name=="STEP")return GlbAnimationInterpolation::Step;
    if(name=="CUBICSPLINE")return GlbAnimationInterpolation::CubicSpline;
    fail("unsupported interpolation");
}
std::size_t factor(GlbAnimationInterpolation value)
{
    if(value==GlbAnimationInterpolation::Linear||value==GlbAnimationInterpolation::Step)return 1;
    if(value==GlbAnimationInterpolation::CubicSpline)return 3;
    fail("invalid interpolation");
}
void validateChannel(const GlbAnimationChannel &channel)
{
    const auto expected=channel.path==GlbAnimationPath::Rotation?4u:
        (channel.path==GlbAnimationPath::Translation||channel.path==GlbAnimationPath::Scale?3u:channel.components);
    if(channel.path!=GlbAnimationPath::Translation&&channel.path!=GlbAnimationPath::Rotation&&
       channel.path!=GlbAnimationPath::Scale&&channel.path!=GlbAnimationPath::Weights)fail("invalid path");
    if(!channel.components||channel.components!=expected||channel.seconds.empty())fail("invalid channel dimensions");
    const auto stride=factor(channel.interpolation);
    if(stride==3&&channel.seconds.size()<2)fail("cubic interpolation needs at least two keys");
    if(channel.values.size()!=checkedProduct(checkedProduct(channel.seconds.size(),stride),channel.components))fail("output size mismatch");
    double previous=-1;
    for(const auto time:channel.seconds)
    {if(!std::isfinite(time)||time<0||time<=previous)fail("key times must be finite and increasing");previous=time;}
    for(const auto value:channel.values)if(!std::isfinite(value))fail("nonfinite output value");
    if(channel.path==GlbAnimationPath::Rotation)
        for(std::size_t i=0;i<channel.seconds.size();++i)
        {
            const auto offset=(i*stride+(stride==3?1:0))*4;
            const auto length=std::hypot(std::hypot(channel.values[offset],channel.values[offset+1]),
                                         std::hypot(channel.values[offset+2],channel.values[offset+3]));
            if(!std::isfinite(length)||std::abs(length-1)>0.02)fail("rotation key is not normalized");
        }
}
std::vector<double> sampleChannel(const GlbAnimationChannel &c,double time)
{
    const auto stride=factor(c.interpolation),width=c.components;
    const auto valueAt=[&](std::size_t key,std::size_t component)
    {return c.values[(key*stride+(stride==3?1:0))*width+component];};
    const auto next=std::upper_bound(c.seconds.begin(),c.seconds.end(),time);
    const auto k=next==c.seconds.begin()?std::size_t{0}:static_cast<std::size_t>(next-c.seconds.begin()-1);
    const auto n=std::min(k+1,c.seconds.size()-1);
    const double dt=c.seconds[n]-c.seconds[k];
    const double u=dt>0?std::clamp((time-c.seconds[k])/dt,0.0,1.0):0;
    std::vector<double> result(width);
    if(c.interpolation==GlbAnimationInterpolation::Linear&&c.path==GlbAnimationPath::Rotation&&k!=n)
    {
        Quaternion a{valueAt(k,0),valueAt(k,1),valueAt(k,2),valueAt(k,3)};
        Quaternion b{valueAt(n,0),valueAt(n,1),valueAt(n,2),valueAt(n,3)};
        const auto q=character_detail::slerp(a.normalized(),b.normalized(),u);return {q.x,q.y,q.z,q.w};
    }
    for(std::size_t j=0;j<width;++j)
    {
        const auto a=valueAt(k,j),b=valueAt(n,j);
        if(k==n||c.interpolation==GlbAnimationInterpolation::Step)result[j]=a;
        else if(c.interpolation==GlbAnimationInterpolation::Linear)result[j]=std::lerp(a,b,u);
        else
        {
            const auto u2=u*u,u3=u2*u;
            const auto out=c.values[(k*3+2)*width+j],in=c.values[(n*3)*width+j];
            result[j]=(2*u3-3*u2+1)*a+(u3-2*u2+u)*dt*out+(-2*u3+3*u2)*b+(u3-u2)*dt*in;
        }
        if(!std::isfinite(result[j]))fail("animation sample overflow");
    }
    if(c.path==GlbAnimationPath::Rotation)
    {const auto q=Quaternion{result[0],result[1],result[2],result[3]}.normalized();result={q.x,q.y,q.z,q.w};}
    return result;
}
} // namespace

GlbAnimationSource::GlbAnimationSource(std::string name,std::vector<GlbAnimationChannel> channels)
    :name_(std::move(name)),channels_(std::move(channels))
{
    if(name_.size()>4096||name_.find('\0')!=name_.npos||channels_.empty())fail("invalid animation name or empty channels");
    std::set<std::pair<std::size_t,GlbAnimationPath>> targets;
    for(const auto &channel:channels_)
    {validateChannel(channel);if(!targets.emplace(channel.sourceNode,channel.path).second)fail("duplicate animation target");duration_=std::max(duration_,channel.seconds.back());}
}
std::string_view GlbAnimationSource::name() const noexcept {return name_;}
double GlbAnimationSource::duration() const noexcept {return duration_;}
std::span<const GlbAnimationChannel> GlbAnimationSource::channels() const noexcept {return channels_;}
std::vector<GlbAnimationValue> GlbAnimationSource::sample(double seconds,AnimationWrap wrap) const
{
    if(!std::isfinite(seconds))fail("sample time must be finite");
    if(wrap!=AnimationWrap::Clamp&&wrap!=AnimationWrap::Loop)fail("invalid wrap mode");
    const auto time=duration_==0?0:character_detail::sampleTime(seconds,duration_,wrap==AnimationWrap::Loop);
    std::vector<GlbAnimationValue> result;result.reserve(channels_.size());
    for(const auto &channel:channels_)result.push_back({channel.sourceNode,channel.path,sampleChannel(channel,time)});
    return result;
}

std::vector<GlbAnimationSource> readGlbAnimationSources(std::span<const std::uint8_t> bytes,
                                                       const GlbAnimationImportOptions &options)
{
    if(!options.maximumAnimations||options.maximumAnimations>100000||!options.maximumChannels||options.maximumChannels>1000000||
       !options.maximumSamplers||options.maximumSamplers>1000000||!options.maximumDecodedValues||options.maximumDecodedValues>100000000||
       !options.asset.maximumNodes||!options.asset.maximumMorphsPerMesh)fail("invalid animation limits");
    const auto doc=GlbDocument::parse(bytes,options.asset.document);ii::json::Limits limits;
    limits.input_bytes=options.asset.document.maximumJsonBytes;limits.string_bytes=limits.input_bytes;limits.items=options.asset.document.maximumJsonItems;
    const auto parsed=ii::json::parse(doc.json(),limits);if(!parsed)fail("invalid JSON");
    const auto &root=*parsed;const auto &nodes=list(root,"nodes"),&meshes=list(root,"meshes"),&animations=list(root,"animations");
    for(const auto &entry:list(root,"extensionsRequired"))
        if(text(entry)!="KHR_mesh_quantization")fail("required GLB extension is unsupported");
    if(nodes.size()>options.asset.maximumNodes||animations.size()>options.maximumAnimations)fail("node or animation count limit");
    std::vector<GlbAnimationSource> result;result.reserve(animations.size());std::size_t channels=0,samplers=0,decoded=0;
    for(const auto &animation:animations)
    {
        noExtensions(animation);const auto &cs=array(required(animation,"channels")),&ss=array(required(animation,"samplers"));
        if(cs.empty()||ss.empty())fail("animation has no channels or samplers");
        if(channels>options.maximumChannels||cs.size()>options.maximumChannels-channels||
           samplers>options.maximumSamplers||ss.size()>options.maximumSamplers-samplers)fail("channel or sampler count limit");
        channels+=cs.size();samplers+=ss.size();
        std::vector<GlbAnimationChannel> output;output.reserve(cs.size());
        for(const auto &entry:cs)
        {
            noExtensions(entry);const auto index=integer(required(entry,"sampler"));if(index>=ss.size())fail("invalid sampler reference");
            const auto &sampler=ss[index],&target=required(entry,"target");noExtensions(sampler);noExtensions(target);
            const auto node=integer(required(target,"node"));if(node>=nodes.size())fail("invalid target node");
            const auto p=path(text(required(target,"path")));if(p!=GlbAnimationPath::Weights&&field(nodes[node],"matrix"))fail("animated TRS node uses a matrix");
            const auto width=p==GlbAnimationPath::Rotation?4u:(p==GlbAnimationPath::Weights?morphCount(nodes[node],meshes,options.asset.maximumMorphsPerMesh):3u);
            const auto mode=interpolation(sampler);const auto multiple=factor(mode);
            const auto input=integer(required(sampler,"input")),value=integer(required(sampler,"output"));
            checkAccessorExtensions(root,input);checkAccessorExtensions(root,value);
            const auto i=doc.accessor(input),o=doc.accessor(value);
            if(i.type!=GlbAccessorType::Scalar||i.component!=GlbComponentType::Float||i.normalized||!i.count)fail("invalid time accessor");
            if((p==GlbAnimationPath::Translation||p==GlbAnimationPath::Scale)&&
               (o.type!=GlbAccessorType::Vec3||o.component!=GlbComponentType::Float||o.normalized))fail("invalid vector output accessor");
            if(p==GlbAnimationPath::Rotation&&(o.type!=GlbAccessorType::Vec4||!normalizedComponent(o)))fail("invalid rotation output accessor");
            if(p==GlbAnimationPath::Weights&&(o.type!=GlbAccessorType::Scalar||!normalizedComponent(o)))fail("invalid weights output accessor");
            const auto requiredValues=checkedProduct(checkedProduct(i.count,multiple),width);
            if(checkedProduct(o.count,o.components)!=requiredValues)fail("output accessor count mismatch");
            consume(decoded,i.count,options.maximumDecodedValues);consume(decoded,requiredValues,options.maximumDecodedValues);
            output.push_back({node,p,mode,width,doc.readFloats(input),doc.readFloats(value)});
        }
        const auto *name=field(animation,"name");result.emplace_back(name?std::string(text(*name)):"Animation_"+std::to_string(result.size()),std::move(output));
    }
    return result;
}

GlbAnimatedImport importObject3DGlbWithAnimations(const Object3DReference &engine,
    std::span<const std::uint8_t> bytes,const GlbAnimationImportOptions &options)
{
    auto animations=readGlbAnimationSources(bytes,options);auto assetOptions=options.asset;
    assetOptions.allowDiscardAnimations=true;std::vector<GlbAnimationNodeBinding> nodes;
    auto asset=importObject3DGlbMapped(engine,bytes,assetOptions,nodes);
    if(!animations.empty())
    {
        const auto i=std::find(asset.warnings.begin(),asset.warnings.end(),"GLB animations were explicitly discarded");
        if(i!=asset.warnings.end())asset.warnings.erase(i);
    }
    return {std::move(asset),std::move(animations),std::move(nodes)};
}
} // namespace iiActorObject
