#include "Object3DInterchange.h"
#include "RigSampleCatalogCodec.h"
#include "Object3DExternalProtocol.h"
#include "Object3DMorphChannels.h"
#include "Object3DFbxTangents.h"
#include "Object3DFbxAnimation.h"
#include "Object3DProgressiveMorph.h"
#include "Object3DProject.h"
#include "Object3DNames.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>
#include <map>
#include <set>
#include <variant>

namespace iiActorObject
{
namespace
{
using Bytes = std::vector<std::uint8_t>;
void integer(Bytes &b, std::uint64_t v, unsigned count)
{
    for (unsigned i = 0; i < count; ++i)
        b.push_back(static_cast<std::uint8_t>(v >> (i * 8)));
}
void patch32(Bytes &b, std::size_t offset, std::size_t value)
{
    if (value > std::numeric_limits<std::uint32_t>::max())
        throw std::overflow_error("FBX 7.4 file exceeds four GiB");
    for (unsigned i = 0; i < 4; ++i)
        b[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
struct Property final
{
    Bytes bytes;
};
Property prop(std::int32_t v)
{
    Property p{{'I'}};
    integer(p.bytes, static_cast<std::uint32_t>(v), 4);
    return p;
}
Property prop(std::int64_t v)
{
    Property p{{'L'}};
    integer(p.bytes, static_cast<std::uint64_t>(v), 8);
    return p;
}
Property prop(double v)
{
    if (!std::isfinite(v))
        throw std::invalid_argument("Nonfinite FBX number");
    Property p{{'D'}};
    integer(p.bytes, std::bit_cast<std::uint64_t>(v), 8);
    return p;
}
Property prop(std::string_view v)
{
    Property p{{'S'}};
    if (v.size() > 0xffffffffu)
        throw std::overflow_error("FBX string");
    integer(p.bytes, v.size(), 4);
    p.bytes.insert(p.bytes.end(), v.begin(), v.end());
    return p;
}
Property blob(std::span<const std::uint8_t> bytes)
{
    if(bytes.size()>0xffffffffu)throw std::length_error("FBX metadata blob limit");
    Property result{{'R'}};integer(result.bytes,bytes.size(),4);
    result.bytes.insert(result.bytes.end(),bytes.begin(),bytes.end());return result;
}
Property packed(std::span<const std::int32_t> values)
{
    Bytes bytes;for(const auto value:values)integer(bytes,static_cast<std::uint32_t>(value),4);return blob(bytes);
}
Property array(std::span<const double> values)
{
    if (values.size() > 0xffffffffu / 8)
        throw std::overflow_error("FBX array");
    Property p{{'d'}};
    integer(p.bytes, values.size(), 4);
    integer(p.bytes, 0, 4);
    integer(p.bytes, values.size() * 8, 4);
    for (double v : values)
    {
        if (!std::isfinite(v))
            throw std::invalid_argument("Nonfinite FBX array");
        integer(p.bytes, std::bit_cast<std::uint64_t>(v), 8);
    }
    return p;
}
Property array(std::span<const std::int32_t> values)
{
    if (values.size() > 0xffffffffu / 4)
        throw std::overflow_error("FBX array");
    Property p{{'i'}};
    integer(p.bytes, values.size(), 4);
    integer(p.bytes, 0, 4);
    integer(p.bytes, values.size() * 4, 4);
    for (auto v : values)
        integer(p.bytes, static_cast<std::uint32_t>(v), 4);
    return p;
}
Property array(std::span<const std::int64_t> values)
{
    if(values.size()>0xffffffffu/8)throw std::overflow_error("FBX array");
    Property p{{'l'}}; integer(p.bytes,values.size(),4);integer(p.bytes,0,4);integer(p.bytes,values.size()*8,4);
    for(const auto v:values)integer(p.bytes,static_cast<std::uint64_t>(v),8);
    return p;
}
Property array(std::span<const float> values)
{
    if(values.size()>0xffffffffu/4)throw std::overflow_error("FBX array");
    Property p{{'f'}}; integer(p.bytes,values.size(),4);integer(p.bytes,0,4);integer(p.bytes,values.size()*4,4);
    for(const auto v:values)
    {
        if(!std::isfinite(v))throw std::invalid_argument("Nonfinite FBX array");
        integer(p.bytes,std::bit_cast<std::uint32_t>(v),4);
    }
    return p;
}
struct Node final
{
    std::string name;
    std::vector<Property> properties;
    std::vector<Node> children;
};
bool sameNode(const Node &a,const Node &b,std::size_t &work)
{
    constexpr std::size_t limit=128'000'000;
    if(a.name!=b.name||a.properties.size()!=b.properties.size()||a.children.size()!=b.children.size())return false;
    for(std::size_t i=0;i<a.properties.size();++i)
    {
        const auto &left=a.properties[i].bytes,&right=b.properties[i].bytes;
        if(left.size()!=right.size())return false;
        if(left.size()>limit-work)throw std::length_error("Progressive FBX shared geometry comparison budget");
        work+=left.size();if(left!=right)return false;
    }
    for(std::size_t i=0;i<a.children.size();++i)if(!sameNode(a.children[i],b.children[i],work))return false;
    return true;
}
void encode(Bytes &out, const Node &n)
{
    if (n.name.size() > 255 || n.properties.size() > 0xffffffffu)
        throw std::overflow_error("FBX node header");
    const auto start = out.size();
    out.resize(start + 13, 0);
    out[start + 12] = static_cast<std::uint8_t>(n.name.size());
    out.insert(out.end(), n.name.begin(), n.name.end());
    const auto beginProperties = out.size();
    for (const auto &p : n.properties)
        out.insert(out.end(), p.bytes.begin(), p.bytes.end());
    patch32(out, start + 4, n.properties.size());
    patch32(out, start + 8, out.size() - beginProperties);
    for (const auto &c : n.children)
        encode(out, c);
    if (!n.children.empty())
        out.resize(out.size() + 13, 0);
    patch32(out, start, out.size());
}
Node value(std::string n, Property p)
{
    return {std::move(n), {std::move(p)}, {}};
}
Node pInt(std::string_view n, std::int32_t v)
{
    return {"P", {prop(n), prop("int"), prop("Integer"), prop(""), prop(v)}, {}};
}
Node pNumber(std::string_view n, double v)
{
    return {"P", {prop(n), prop("double"), prop("Number"), prop(""), prop(v)}, {}};
}
Node pEnum(std::string_view name,std::int32_t value)
{return {"P",{prop(name),prop("enum"),prop(""),prop(""),prop(value)}, {}};}
Node pVector(std::string_view n, std::string_view type, Vector3 v)
{
    return {"P", {prop(n), prop(type), prop(""), prop("A"), prop(v.x), prop(v.y), prop(v.z)}, {}};
}
std::string objectName(std::string_view name, std::string_view type)
{
    std::string out(name);
    out.append("\0\1", 2);
    out += type;
    return out;
}
Vector3 euler(Quaternion q)
{
    q = q.normalized();
    const auto degrees = 180 / std::numbers::pi;
    return {degrees * std::atan2(2 * (q.w * q.x + q.y * q.z), 1 - 2 * (q.x * q.x + q.y * q.y)),
            degrees * std::asin(std::clamp(2 * (q.w * q.y - q.z * q.x), -1.0, 1.0)),
            degrees * std::atan2(2 * (q.w * q.z + q.x * q.y), 1 - 2 * (q.y * q.y + q.z * q.z))};
}
Node model(std::int64_t id, std::string_view name, std::string_view type, Transform t = {})
{
    return {"Model",
            {prop(id), prop(objectName(name, "Model")), prop(type)},
            {value("Version", prop(232)),
             {"Properties70",
              {},
              {pVector("Lcl Translation", "Lcl Translation", t.position),
               pVector("Lcl Rotation", "Lcl Rotation", euler(t.rotation)),
               pVector("Lcl Scaling", "Lcl Scaling", t.scale), pInt("RotationOrder", 0),
               pInt("InheritType", 1)}},
             value("Culling", prop("CullingOff"))}};
}
Node externalText(std::string_view name,std::string_view text)
{return {"P",{prop(name),prop("KString"),prop(""),prop("U"),prop(text)}, {}};}
Node externalInteger(std::string_view name,std::int32_t number)
{return {"P",{prop(name),prop("int"),prop("Integer"),prop("U"),prop(number)}, {}};}
Node externalNumber(std::string_view name,double number)
{return {"P",{prop(name),prop("double"),prop("Number"),prop("U"),prop(number)}, {}};}
std::int32_t externalIndex(std::size_t index)
{if(index>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))throw std::length_error("FBX external source index range");return static_cast<std::int32_t>(index);}
void xyz(std::vector<double> &v, Vector3 p)
{
    v.insert(v.end(), {p.x, p.y, p.z});
}
Node layerElement(std::string_view type,std::int32_t index=0)
{
    return {"LayerElement", {}, {value("Type", prop(type)), value("TypedIndex", prop(index))}};
}
std::vector<std::int32_t> indices(std::span<const std::uint32_t> values)
{
    std::vector<std::int32_t> out;for(auto v:values)out.push_back(static_cast<std::int32_t>(v));return out;
}
Node authoring(const MeshTopology &topology)
{
    const auto &d=topology.layout->definition();std::vector<std::int32_t> emptyWeights;
    for(std::size_t p=0;p<topology.points.size();++p)if(topology.points[p].weights.empty())emptyWeights.push_back(static_cast<std::int32_t>(p));
    std::vector<std::int32_t> layouts,sizes,polygons;
    for(const auto &v:d.layouts)layouts.insert(layouts.end(),{static_cast<std::int32_t>(v.position),static_cast<std::int32_t>(v.textureCoordinate),static_cast<std::int32_t>(v.normal)});
    for(const auto &p:d.polygons){sizes.push_back(static_cast<std::int32_t>(p.size()));for(auto i:p)polygons.push_back(static_cast<std::int32_t>(i));}
    Bytes groups;for(const auto &g:d.polygonGroups)
    {if(g.size()>1024)throw std::length_error("FBX source group label limit");integer(groups,g.size(),4);groups.insert(groups.end(),g.begin(),g.end());}
    Node result{"iiActorObjectTopology",{},
        {value("Version",prop(1)),value("Format",prop(d.sourceFormat)),value("Element",prop(static_cast<std::int64_t>(d.sourceElement))),
         value("PositionCount",prop(static_cast<std::int64_t>(d.positionCount))),
         value("UvCount",prop(static_cast<std::int64_t>(d.textureCoordinateCount))),value("NormalCount",prop(static_cast<std::int64_t>(d.normalCount))),
         value("Reverse",prop(d.reverseWinding?1:0)),value("Layouts",packed(layouts)),value("PolygonSizes",packed(sizes)),
         value("Polygons",packed(polygons)),value("Hidden",packed(indices(d.hiddenPolygons))),
         value("RenderToLayout",packed(indices(d.renderToLayout))),value("Materials",packed(indices(d.polygonMaterials))),value("EmptyWeights",packed(emptyWeights)),value("Groups",blob(groups))}};
    for(const auto &uv:topology.additionalUvSets)
        result.children.push_back({"AdditionalUvSet",{}, {value("Name",prop(uv.name)),value("Layouts",packed(indices(uv.layoutIndices)))}});
    return result;
}
} // namespace
struct ProgressiveFbxPlan final
{
    std::vector<std::vector<const ProgressiveMorphChannel *>> channels;
    std::vector<std::set<std::string,std::less<>>> consumedTargets;
    std::vector<std::size_t> representative;
};
struct FbxExportTake final
{
    std::string_view name;
    const AnimationClip *clip;
    std::span<const AnimationMorphBinding> morphBindings;
    double sourceStartSeconds=0;
};
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
static void validateTakeOptions(std::size_t count,const FbxProgressiveTakesExportOptions &options)
{
    if(options.maximumTakes==0||options.maximumTakes>1024||count==0||count>options.maximumTakes||
       !options.animation.indexedBindings.empty())
        throw std::invalid_argument("Invalid progressive FBX take export options");
}
static void validateTakeName(std::string_view name,std::set<std::string,std::less<>> &names)
{
    object3d_detail::validateName(name);
    if(!names.emplace(name).second)throw std::invalid_argument("Duplicate progressive FBX take name");
}
static std::vector<std::uint8_t> exportFbx(const Object3DAsset &,const FbxExportOptions &,
                                           std::span<const FbxExportTake>,const FbxAnimationExportOptions *,
                                           const ProgressiveFbxPlan *);
std::vector<std::uint8_t> exportObject3DFbx(const Object3DAsset &asset)
{
    return exportObject3DFbx(asset,FbxExportOptions{});
}
std::vector<std::uint8_t> exportObject3DFbx(const Object3DAsset &asset,const FbxExportOptions &options)
{
    return exportFbx(asset,options,{},nullptr,nullptr);
}
std::vector<std::uint8_t> exportObject3DFbx(const Object3DAsset &asset,const AnimationClip &clip,
                                            const FbxAnimationExportOptions &options)
{
    const FbxExportTake take{"Take",&clip,options.indexedBindings,0};
    return exportFbx(asset,options.geometry,std::span(&take,1),&options,nullptr);
}
static FbxProgressiveMorphExport exportProgressiveFbx(const Object3DRigHandle &rig,const FbxExportOptions &options,
    std::span<const FbxExportTake> takes,const FbxAnimationExportOptions *animationOptions,bool preserveStarts)
{
    if(!options.externalDocument.empty())throw std::invalid_argument("Progressive FBX external identity is not supported");
    const auto program=inspectObject3DProgressiveMorphRig(rig);
    auto &session=rig.get();const auto &asset=session.object().get();const auto &meshes=asset.data().meshes;
    ProgressiveFbxPlan plan;plan.channels.resize(meshes.size());plan.consumedTargets.resize(meshes.size());
    plan.representative.resize(meshes.size());for(std::size_t i=0;i<meshes.size();++i)plan.representative[i]=i;
    std::map<std::string,std::vector<std::size_t>,std::less<>> membership;
    for(const auto &channel:program.channels)for(const auto &key:channel.keys)membership.try_emplace(key.target);
    std::vector<std::map<std::string_view,std::size_t,std::less<>>> records(meshes.size());
    std::vector<std::map<std::string_view,double,std::less<>>> assetDefaults(meshes.size());
    constexpr std::size_t maximumVisits=128'000'000;std::size_t visits=0;
    for(std::size_t meshIndex=0;meshIndex<meshes.size();++meshIndex)
    {
        const auto &mesh=meshes[meshIndex];
        if(mesh.geometry.morphTargets.size()>maximumVisits-visits)throw std::length_error("Progressive FBX binding visit budget");
        visits+=mesh.geometry.morphTargets.size();std::set<std::string_view,std::less<>> seen;
        for(const auto &morph:mesh.geometry.morphTargets)
            if(auto found=membership.find(morph.name);found!=membership.end()&&seen.insert(morph.name).second)
                found->second.push_back(meshIndex);
        if(mesh.morphDefaults.size()>maximumVisits-visits)throw std::length_error("Progressive FBX defaults visit budget");
        visits+=mesh.morphDefaults.size();
        for(const auto &entry:mesh.morphDefaults)if(membership.contains(entry.name))assetDefaults[meshIndex].emplace(entry.name,entry.value);
        if(mesh.topology)
        {
            if(mesh.topology->morphTargets.size()>maximumVisits-visits)throw std::length_error("Progressive FBX source record visit budget");
            visits+=mesh.topology->morphTargets.size();
            for(const auto &morph:mesh.topology->morphTargets)
                if(membership.contains(morph.name))++records[meshIndex][morph.name];
        }
    }
    std::map<std::vector<std::size_t>,std::size_t> groups;
    std::vector<std::optional<std::size_t>> assigned(meshes.size());std::size_t comparisons=0;
    const auto account=[&](std::size_t count)
    {if(count>maximumVisits-comparisons)throw std::length_error("Progressive FBX mesh binding comparison budget");comparisons+=count;};
    for(const auto &channel:program.channels)
    {
        const std::vector<std::size_t> *owners=nullptr;
        const auto defaultPercent=100.*channel.defaultValue;
        if(!std::isfinite(defaultPercent))throw std::overflow_error("Progressive FBX default percentage overflow");
        double previous=0;bool first=true;
        for(const auto &key:channel.keys)
        {
            const auto &attached=membership.at(key.target);
            account(attached.size());
            if(attached.empty()||(owners&&*owners!=attached))
                throw std::invalid_argument("Progressive FBX keys must address the same set of meshes");
            owners=&attached;const double percentage=100.*key.fullWeight;
            if(!std::isfinite(percentage)||percentage==0||(!first&&percentage<=previous))
                throw std::overflow_error("Progressive FBX full-weight percentage cannot be represented");
            previous=percentage;first=false;
            for(const auto index:attached)plan.consumedTargets[index].insert(key.target);
        }
        const auto [group,created]=groups.try_emplace(*owners,groups.size());(void)created;
        for(const auto index:*owners)
        {
            if(assigned[index]&&*assigned[index]!=group->second)
                throw std::invalid_argument("Overlapping progressive FBX mesh groups cannot share one geometry");
            assigned[index]=group->second;plan.representative[index]=owners->front();
        }
        plan.channels.at(owners->front()).push_back(&channel);
    }
    const auto defaults=session.evaluateNamed({});const auto &names=session.definition().blendShapes;
    for(std::size_t i=0;i<names.size();++i)
    {
        const auto &owners=membership.at(names[i]);account(owners.size());
        for(const auto meshIndex:owners)
        {
            const auto &mesh=meshes[meshIndex];
            const auto found=assetDefaults[meshIndex].find(names[i]);
            const auto actual=found==assetDefaults[meshIndex].end()?0.:found->second,expected=defaults.blendShapes[i];
            if(std::abs(actual-expected)>1e-10*std::max({1.,std::abs(actual),std::abs(expected)}))
                throw std::invalid_argument("Progressive FBX rig and asset morph defaults disagree");
            if(mesh.topology)
            {
                const auto foundRecord=records[meshIndex].find(names[i]);
                if(foundRecord==records[meshIndex].end()||foundRecord->second!=1)
                    throw std::invalid_argument("Progressive FBX export cannot preserve multiple source records for one target");
            }
        }
    }
    FbxProgressiveMorphExport result;
    result.bytes=exportFbx(asset,options,takes,animationOptions,&plan);
    result.warnings.push_back("FBX preserves progressive shape behavior; SDK control and target identifiers, source record IDs and provenance may change when reimported. Colliding ordinary channel names may also change. Retain the SDK project.");
    result.warnings.push_back("Derived shape normals are baked into the FBX; other applications may evaluate values outside the key range differently.");
    if(!takes.empty())
    {
        result.warnings.push_back(preserveStarts?
            "Animation take start times are rounded to FBX ticks; sampled FBX scalar values use float precision.":
            "One animation take is exported with its timeline starting at zero; sampled FBX scalar values use float precision.");
        if(animationOptions->allowDiscardScalarCurves)
            result.warnings.push_back("Unbound non-progressive scalar curves may have been explicitly discarded.");
    }
    else result.warnings.push_back("Progressive rig animation is not included in this static export.");
    return result;
}
FbxProgressiveMorphExport exportObject3DFbxWithProgressiveMorphs(const Object3DRigHandle &rig,const FbxExportOptions &options)
{return exportProgressiveFbx(rig,options,{},nullptr,false);}
FbxProgressiveMorphExport exportObject3DFbxWithProgressiveMorphs(const Object3DRigHandle &rig,
    const AnimationClip &clip,const FbxAnimationExportOptions &options)
{
    const FbxExportTake take{"Take",&clip,options.indexedBindings,0};
    return exportProgressiveFbx(rig,options.geometry,std::span(&take,1),&options,false);
}
FbxProgressiveMorphExport exportObject3DFbxWithProgressiveMorphs(const Object3DRigHandle &rig,
    std::span<const FbxImportedAnimation> takes,const FbxProgressiveTakesExportOptions &options)
{
    validateTakeOptions(takes.size(),options);
    std::set<std::string,std::less<>> names;std::vector<FbxExportTake> requested;requested.reserve(takes.size());
    for(const auto &take:takes)
    {
        validateTakeName(take.name,names);
        if(!take.clip||!std::isfinite(take.sourceStartSeconds))throw std::invalid_argument("Invalid progressive FBX take");
        requested.push_back({take.name,take.clip.get(),take.morphBindings,take.sourceStartSeconds});
    }
    return exportProgressiveFbx(rig,options.animation.geometry,requested,&options.animation,true);
}
FbxProgressiveMorphExport exportObject3DFbxWithProgressiveMorphs(const Object3DRigHandle &rig,
    std::span<const Object3DProjectAnimation> takes,const FbxProgressiveTakesExportOptions &options)
{
    validateTakeOptions(takes.size(),options);
    const auto &object=rig.get().object();(void)object.get();
    std::set<std::string,std::less<>> names;std::vector<FbxExportTake> requested;requested.reserve(takes.size());
    bool missingOrigin=false;
    for(const auto &take:takes)
    {
        validateTakeName(take.name,names);
        if(take.object!=object||!take.clip||
           (take.sourceStartSeconds&&!std::isfinite(*take.sourceStartSeconds)))
            throw std::invalid_argument("Invalid progressive FBX project take");
        missingOrigin=missingOrigin||!take.sourceStartSeconds;
        requested.push_back({take.name,take.clip.get(),take.morphBindings,take.sourceStartSeconds.value_or(0)});
    }
    auto result=exportProgressiveFbx(rig,options.animation.geometry,requested,&options.animation,true);
    if(missingOrigin)result.warnings.push_back("A project take source start time was absent; zero was used.");
    return result;
}
static std::vector<std::uint8_t> exportFbx(const Object3DAsset &asset,const FbxExportOptions &options,
                                           std::span<const FbxExportTake> requestedTakes,const FbxAnimationExportOptions *animationOptions,
                                           const ProgressiveFbxPlan *progressive)
{
    if(asset.data().referencePose)throw std::invalid_argument("Bake or clear the reference pose before FBX export");
    const auto &d = asset.data();
    if(!asset.hasRig())for(const auto &mesh:d.meshes)
        if(mesh.geometry.skinning.method!=SkinningMethod::Linear)
            throw std::invalid_argument("FBX cannot retain a skinning profile without a skin deformer");
    Object3DExternalIdentity external;if(!options.externalDocument.empty())external=makeObject3DExternalIdentity(d,options.externalDocument);
    std::int64_t next = 1000;
    Node objects{"Objects", {}, {}}, connections{"Connections", {}, {}}, pose{"Pose", {}, {}};
    const auto connect = [&](std::int64_t child, std::int64_t parent)
    { connections.children.push_back({"C", {prop("OO"), prop(child), prop(parent)}, {}}); };
    const auto rigId = next++;
    auto rigModel = model(rigId, d.name + (asset.hasRig() ? "Rig" : "Root"), "Null");
    rigModel.children[1].children.push_back(
        {"P", {prop("iiObject3DName"), prop("KString"), prop(""), prop("U"), prop(d.name)}, {}});
    rigModel.children[1].children.push_back(pInt("iiObject3DKind", static_cast<std::int32_t>(d.kind)));
    if(!d.rigSampleCatalogs.empty())
    {
        const auto metadata=object3d_detail::encodeRigSampleCatalogs(d.rigSampleCatalogs);
        rigModel.children.push_back({"iiActorObjectRigSamples",{},
            {value("Version",prop(1)),value("Data",blob(metadata))}});
    }
    if(!external.document.empty())rigModel.children[1].children.push_back(externalText("iiActorObjectDocument",external.document));
    objects.children.push_back(std::move(rigModel));
    connect(rigId, 0);
    const auto poseId = next++;
    pose = {"Pose",
            {prop(poseId), prop(objectName("BindPose", "Pose")), prop("BindPose")},
            {value("Type", prop("BindPose")), value("Version", prop(100)),
             value("NbPoseNodes", prop(static_cast<std::int32_t>(d.bones.size() + d.meshes.size() + 1)))}};
    const auto poseNode = [&](std::int64_t id, const Matrix4 &m)
    {
        pose.children.push_back(
            {"PoseNode", {}, {value("Node", prop(id)), value("Matrix", array(m.elements()))}});
    };
    Pose rest(asset.skeleton());
    const auto worlds = rest.worldMatrices();
    poseNode(rigId, Matrix4{});
    std::vector<std::int64_t> bones(asset.hasRig()?d.bones.size():0);
    for(auto &id:bones){id=next;next+=2;}
    for (std::size_t i = 0; asset.hasRig() && i < d.bones.size(); ++i)
    {
        const auto id = bones[i], attribute = id+1;
        const auto &bone = d.bones[i];
        auto boneModel = model(id, bone.name, "LimbNode", bone.bindTransform);
        if(!external.document.empty())boneModel.children[1].children.push_back(externalInteger("iiActorObjectSourceBone",externalIndex(i)));
        std::size_t semanticIndex = 0;
        for (const auto &s : d.semantics)
            if (s.bone == i)
                boneModel.children[1].children.push_back(
                    {"P",
                     {prop("iiActorSemantic." + std::to_string(semanticIndex++)), prop("KString"), prop(""),
                      prop("U"), prop(s.name)},
                     {}});
        objects.children.push_back(std::move(boneModel));
        objects.children.push_back(
            {"NodeAttribute",
             {prop(attribute), prop(objectName(bone.name, "NodeAttribute")), prop("LimbNode")},
             {value("TypeFlags", prop("Skeleton")), {"Properties70", {}, {pNumber("Size", .01)}}}});
        if(!external.document.empty())objects.children.back().children[1].children.push_back(externalInteger("iiActorObjectSourceBone",externalIndex(i)));
        connect(attribute, id);
        connect(id, bone.parent == NoBone ? rigId : bones[bone.parent]);
        poseNode(id, worlds[i]);
    }
    std::vector<std::int64_t> videos;
    for(std::size_t i=0;i<d.images.size();++i)
    {
        const auto &image=d.images[i];const auto video=next++;videos.push_back(video);
        const auto filename="iiActorObjectImage"+std::to_string(i)+".png";
        objects.children.push_back({"Video",{prop(video),prop(objectName(image.name(),"Video")),prop("Clip")},
            {value("Type",prop("Clip")),{"Properties70",{},{}},value("UseMipMap",prop(0)),value("Filename",prop(filename)),value("RelativeFilename",prop(filename)),value("Content",blob(image.encoded()))}});
    }
    std::map<std::pair<std::size_t,std::string>,std::int64_t> textures;
    const auto textureFor=[&](std::size_t i,const std::string &uvSet)
    {
        const auto key=std::pair{i,uvSet};if(const auto found=textures.find(key);found!=textures.end())return found->second;
        const auto &image=d.images.at(i);const auto video=videos.at(i),texture=next++;
        const auto filename="iiActorObjectImage"+std::to_string(i)+".png";
        objects.children.push_back({"Texture",{prop(texture),prop(objectName(image.name(),"Texture")),prop("")},
            {value("Type",prop("TextureVideoClip")),value("Version",prop(202)),value("TextureName",prop(objectName(image.name(),"Texture"))),
             {"Properties70",{},{externalText("iiActorObjectImageName",image.name()),pEnum("WrapModeU",0),pEnum("WrapModeV",0)}},
             value("Media",prop(objectName(image.name(),"Video"))),value("FileName",prop(filename)),value("RelativeFilename",prop(filename)),
             {"ModelUVTranslation",{prop(0.0),prop(0.0)},{}},{"ModelUVScaling",{prop(1.0),prop(1.0)},{}},value("Texture_Alpha_Source",prop("None")),
             {"Cropping",{prop(0),prop(0),prop(0),prop(0)}, {}}}});
        if(!uvSet.empty())objects.children.back().children[3].children.push_back(externalText("UVSet",uvSet));
        connect(video,texture);
        textures.emplace(key,texture);return texture;
    };
    for(std::size_t i=0;i<d.images.size();++i)(void)textureFor(i,{});
    std::vector<std::int64_t> materials;
    for (std::size_t materialIndex=0;materialIndex<d.materials.size();++materialIndex)
    {
        const auto &m=d.materials[materialIndex];
        const auto opacity=effectiveAlphaMode(m)==Object3DAlphaMode::Opaque?1.0:m.baseColor[3];
        const auto id = next++;
        materials.push_back(id);
        objects.children.push_back(
            {"Material",
             {prop(id), prop(objectName(m.name, "Material")), prop("")},
             {value("Version", prop(102)),
              value("ShadingModel", prop("phong")),
              value("MultiLayer", prop(0)),
              {"Properties70",
               {},
               {pVector("DiffuseColor", "Color", {m.baseColor[0], m.baseColor[1], m.baseColor[2]}),
                pNumber("DiffuseFactor", 1), pNumber("Shininess", (1 - m.roughness)*(1 - m.roughness)*100),
                pNumber("Opacity", opacity), pVector("TransparentColor", "Color", {1, 1, 1}),
                pNumber("TransparencyFactor", 1 - opacity), external.document.empty()?pNumber("iiMetallic", m.metallic):externalNumber("iiMetallic",m.metallic),
                external.document.empty()?pNumber("iiRoughness", m.roughness):externalNumber("iiRoughness",m.roughness)}}}});
        if(!external.document.empty())objects.children.back().children.back().children.push_back(externalInteger("iiActorObjectSourceMaterial",externalIndex(materialIndex)));
        if(m.surface!=Object3DMaterialSurface{})
        {
            auto &properties=objects.children.back().children.back().children;
            properties.push_back(externalInteger("iiAlphaMode",static_cast<std::int32_t>(m.surface.alphaMode)));
            properties.push_back(externalNumber("iiAlphaCutoff",m.surface.alphaCutoff));
            properties.push_back(externalInteger("iiDoubleSided",m.surface.doubleSided?1:0));
            properties.push_back(externalNumber("iiBaseColorAlpha",m.baseColor[3]));
        }
        if(m.normalScale!=1)objects.children.back().children.back().children.push_back(externalNumber("iiNormalScale",m.normalScale));
        if(m.baseColorImage)connections.children.push_back({"C",{prop("OP"),prop(textureFor(*m.baseColorImage,m.baseColorUvSet)),prop(id),prop("DiffuseColor")},{}});
        if(m.normalImage)connections.children.push_back({"C",{prop("OP"),prop(textureFor(*m.normalImage,m.normalUvSet)),prop(id),prop("NormalMap")},{}});
    }
    object3d_detail::FbxMorphChannels morphChannels;
    object3d_detail::FbxProgressiveChannels progressiveChannels;
    std::size_t progressiveShapeWork=0,sharedWork=0;
    std::vector<std::size_t> geometryNodeIndices(d.meshes.size());
    std::vector<std::int64_t> geometryIds(d.meshes.size());
    const auto morphsFor=[](const Object3DMesh &part)
    {
        std::vector<TopologyMorph> result;
        if(part.topology)return part.topology->morphTargets;
        for(const auto &m:part.geometry.morphTargets)
        {
            TopologyMorph target;target.name=m.name;target.normalDeltas.emplace();
            for(const auto &v:m.deltas)
            {
                target.deltas.push_back({static_cast<std::uint32_t>(v.vertex),v.position});
                if(v.normal!=Vector3{})target.normalDeltas->push_back({static_cast<std::uint32_t>(v.vertex),v.normal});
            }
            result.push_back(std::move(target));
        }
        return result;
    };
    const auto accountShared=[&](std::size_t count)
    {if(count>128'000'000-sharedWork)throw std::length_error("Progressive FBX shared mesh comparison budget");sharedWork+=count;};
    for (std::size_t meshIndex=0;meshIndex<d.meshes.size();++meshIndex)
    {
        const auto &mesh=d.meshes[meshIndex];
        const auto &g = mesh.geometry;
        const auto leader=progressive?progressive->representative[meshIndex]:meshIndex;
        const bool follower=leader!=meshIndex;
        std::map<std::string_view,std::size_t,std::less<>> logicalMorphIndices;
        for(std::size_t target=0;target<g.morphTargets.size();++target)
            logicalMorphIndices.emplace(g.morphTargets[target].name,target);
        const auto *source=mesh.topology?&*mesh.topology:nullptr;
        const auto count=source?source->points.size():g.vertices.size();
        if (count > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
            throw std::overflow_error("FBX vertex index");
        const auto geometry = next++, meshModel = next++, skin = next++;
        std::vector<double> positions, normals, uvs;
        std::vector<std::int32_t> polygons,uvIndices,normalIndices,faceMaterials;
        if(source)
        {
            const auto &definition=source->layout->definition();
            if(definition.layouts.size()>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))throw std::length_error("FBX layout index limit");
            for(const auto &p:source->points)xyz(positions,p.position);
            for(const auto &n:source->normals)xyz(normals,n);
            for(const auto uv:source->textureCoordinates)uvs.insert(uvs.end(),{uv.u,uv.v});
            if(normals.empty())
            {
                std::vector<Vector3> generated(definition.layouts.size(),{0,0,1});
                for(std::size_t r=0;r<definition.renderToLayout.size();++r)generated[definition.renderToLayout[r]]=g.vertices[r].normal;
                for(const auto n:generated)xyz(normals,n);
            }
            std::set<std::uint32_t> hidden(definition.hiddenPolygons.begin(),definition.hiddenPolygons.end());
            for(std::size_t f=0;f<definition.polygons.size();++f)
            {
                if(hidden.contains(static_cast<std::uint32_t>(f)))continue;
                auto face=definition.polygons[f];if(definition.reverseWinding)std::reverse(face.begin(),face.end());
                for(std::size_t c=0;c<face.size();++c)
                {
                    const auto v=definition.layouts[face[c]];const auto p=static_cast<std::int32_t>(v.position);
                    polygons.push_back(c+1==face.size()?-p-1:p);
                    normalIndices.push_back(static_cast<std::int32_t>(v.normal==NoTopologyIndex?face[c]:v.normal));
                    if(v.textureCoordinate!=NoTopologyIndex)uvIndices.push_back(static_cast<std::int32_t>(v.textureCoordinate));
                }
                faceMaterials.push_back(static_cast<std::int32_t>(definition.polygonMaterials.empty()?mesh.material:definition.polygonMaterials[f]));
            }
        }
        else
        {
            for(const auto &v:g.vertices){xyz(positions,v.position);xyz(normals,v.normal);}
            for(const auto uv:mesh.textureCoordinates)uvs.insert(uvs.end(),{uv.u,uv.v});
            for(std::size_t i=0;i<g.triangles.size();++i)
            {
                const auto v=static_cast<std::int32_t>(g.triangles[i]);polygons.push_back(i%3==2?-v-1:v);
                normalIndices.push_back(v);uvIndices.push_back(v);
                if(i%3==0)faceMaterials.push_back(static_cast<std::int32_t>(mesh.triangleMaterials.empty()?mesh.material:mesh.triangleMaterials[i/3]));
            }
        }
        Node geom{"Geometry",
                  {prop(geometry), prop(objectName(mesh.name, "Geometry")), prop("Mesh")},
                  {value("GeometryVersion", prop(124)),
                   value("Vertices", array(positions)),
                   value("PolygonVertexIndex", array(polygons)),
                   {"LayerElementNormal",
                    {prop(0)},
                    {value("Version", prop(101)), value("Name", prop("")),
                     value("MappingInformationType", prop("ByPolygonVertex")),
                     value("ReferenceInformationType", prop("IndexToDirect")), value("Normals", array(normals)),value("NormalsIndex",array(normalIndices))}},
                   {"LayerElementMaterial",
                    {prop(0)},
                    {value("Version", prop(101)), value("Name", prop("")),
                     value("MappingInformationType", prop("ByPolygon")),
                     value("ReferenceInformationType", prop("IndexToDirect")),
                     value("Materials", array(faceMaterials))}}}};
        Node layer{"Layer",
                   {prop(0)},
                   {value("Version", prop(100)), layerElement("LayerElementNormal"),
                    layerElement("LayerElementMaterial")}};
        if (!uvs.empty())
        {
            geom.children.push_back({"LayerElementUV",
                                     {prop(0)},
                                     {value("Version", prop(101)), value("Name", prop(mesh.primaryUvName.empty()?"UVMap":mesh.primaryUvName)),
                                      value("MappingInformationType", prop("ByPolygonVertex")),
                                      value("ReferenceInformationType", prop("IndexToDirect")),
                                      value("UV", array(uvs)), value("UVIndex", array(uvIndices))}});
            layer.children.push_back(layerElement("LayerElementUV"));
        }
        for(std::size_t set=0;set<mesh.additionalUvSets.size();++set)
        {
            const auto &renderSet=mesh.additionalUvSets[set];std::vector<double> values;
            std::vector<std::int32_t> setIndices;
            if(source)
            {
                const auto &uv=source->additionalUvSets[set];const auto &definition=source->layout->definition();
                for(const auto value:uv.coordinates)values.insert(values.end(),{value.u,value.v});
                const std::set<std::uint32_t> hidden(definition.hiddenPolygons.begin(),definition.hiddenPolygons.end());
                for(std::size_t f=0;f<definition.polygons.size();++f)
                {
                    if(hidden.contains(static_cast<std::uint32_t>(f)))continue;
                    auto face=definition.polygons[f];if(definition.reverseWinding)std::reverse(face.begin(),face.end());
                    for(const auto layout:face)setIndices.push_back(static_cast<std::int32_t>(uv.layoutIndices.at(layout)));
                }
            }
            else
            {
                for(const auto value:renderSet.coordinates)values.insert(values.end(),{value.u,value.v});
                for(const auto vertex:g.triangles)setIndices.push_back(static_cast<std::int32_t>(vertex));
            }
            const auto id=static_cast<std::int32_t>(set+1);
            geom.children.push_back({"LayerElementUV",{prop(id)},
                {value("Version",prop(101)),value("Name",prop(renderSet.name)),
                 value("MappingInformationType",prop("ByPolygonVertex")),
                 value("ReferenceInformationType",prop("IndexToDirect")),value("UV",array(values)),value("UVIndex",array(setIndices))}});
        }
        std::vector<bool> hasTangent(mesh.additionalUvSets.size()+1),hasBinormal(mesh.additionalUvSets.size()+1);
        for(const auto &basis:object3d_detail::buildFbxTangentLayers(d,mesh))
        {
            if(!basis.status.empty())
                geom.children.push_back({"iiActorObjectTangentStatus",{},
                    {value("UVIndex",prop(static_cast<std::int32_t>(basis.uvIndex))),
                     value("UVName",prop(basis.uvName)),value("Reason",prop(basis.status))}});
            std::vector<double> tangents,binormals;tangents.reserve(basis.tangents.size()*3);binormals.reserve(basis.binormals.size()*3);
            for(const auto value:basis.tangents)xyz(tangents,value);
            for(const auto value:basis.binormals)xyz(binormals,value);
            const auto id=static_cast<std::int32_t>(basis.uvIndex);
            if(!tangents.empty())
            {
                Node tangent{"LayerElementTangent",{prop(id)},
                    {value("Version",prop(101)),value("Name",prop(basis.uvName)),
                     value("MappingInformationType",prop("ByPolygonVertex")),
                     value("ReferenceInformationType",prop("Direct")),value("Tangents",array(tangents))}};
                if(!basis.handedness.empty())tangent.children.push_back(value("TangentsW",array(basis.handedness)));
                geom.children.push_back(std::move(tangent));hasTangent[basis.uvIndex]=true;
            }
            if(!binormals.empty())
            {
                Node binormal{"LayerElementBinormal",{prop(id)},
                    {value("Version",prop(101)),value("Name",prop(basis.uvName)),
                     value("MappingInformationType",prop("ByPolygonVertex")),
                     value("ReferenceInformationType",prop("Direct")),value("Binormals",array(binormals))}};
                if(!basis.binormalW.empty())binormal.children.push_back(value("BinormalsW",array(basis.binormalW)));
                geom.children.push_back(std::move(binormal));hasBinormal[basis.uvIndex]=true;
            }
        }
        if(source)geom.children.push_back(authoring(*source));
        if(hasTangent[0])layer.children.push_back(layerElement("LayerElementTangent"));
        if(hasBinormal[0])layer.children.push_back(layerElement("LayerElementBinormal"));
        geom.children.push_back(std::move(layer));
        for(std::size_t set=0;set<mesh.additionalUvSets.size();++set)
        {
            const auto id=static_cast<std::int32_t>(set+1);
            Node extra{"Layer",{prop(id)},{value("Version",prop(100)),layerElement("LayerElementUV",id)}};
            if(hasTangent[set+1])extra.children.push_back(layerElement("LayerElementTangent",id));
            if(hasBinormal[set+1])extra.children.push_back(layerElement("LayerElementBinormal",id));
            geom.children.push_back(std::move(extra));
        }
        if(follower)
        {
            const auto &original=objects.children.at(geometryNodeIndices[leader]);
            if(geom.children.size()!=original.children.size())
                throw std::invalid_argument("Shared progressive FBX meshes have different geometry");
            for(std::size_t i=0;i<geom.children.size();++i)
                if(!sameNode(geom.children[i],original.children[i],sharedWork))
                    throw std::invalid_argument("Shared progressive FBX meshes have different geometry");
            const auto &first=d.meshes[leader];const auto *firstSource=first.topology?&*first.topology:nullptr;
            if(bool(source)!=bool(firstSource)||count!=(firstSource?firstSource->points.size():first.geometry.vertices.size())||
               (source?source->skinning:g.skinning)!=(firstSource?firstSource->skinning:first.geometry.skinning))
                throw std::invalid_argument("Shared progressive FBX meshes have different skinning");
            accountShared(count);
            for(std::size_t i=0;i<count;++i)
            {
                const auto &a=source?source->points[i].weights:g.vertices[i].weights;
                const auto &b=firstSource?firstSource->points[i].weights:first.geometry.vertices[i].weights;
                if(a.size()!=b.size())throw std::invalid_argument("Shared progressive FBX meshes have different skin weights");
                accountShared(a.size());
                for(std::size_t w=0;w<a.size();++w)if(a[w].bone!=b[w].bone||a[w].weight!=b[w].weight)
                    throw std::invalid_argument("Shared progressive FBX meshes have different skin weights");
            }
        }
        else
        {
            geometryNodeIndices[meshIndex]=objects.children.size();geometryIds[meshIndex]=geometry;
            objects.children.push_back(std::move(geom));
        }
        auto modelNode=model(meshModel,mesh.name,"Mesh");
        if(!external.document.empty())
        {
            const auto &identity=external.meshes[meshIndex];auto &props=modelNode.children[1].children;
            props.push_back(externalText("iiActorObjectDocument",external.document));
            props.push_back(externalInteger("iiActorObjectSourceMesh",externalIndex(meshIndex)));props.push_back(externalText("iiActorObjectSourcePointIds",object3d_detail::externalIds(identity.points)));
            std::vector<std::uint32_t> faceIds,cornerIds;
            if(source){const auto &definition=source->layout->definition();std::set<std::uint32_t> hidden(definition.hiddenPolygons.begin(),definition.hiddenPolygons.end());for(std::size_t f=0;f<definition.polygons.size();++f)if(!hidden.contains(static_cast<std::uint32_t>(f))){faceIds.push_back(static_cast<std::uint32_t>(f));auto face=definition.polygons[f];if(definition.reverseWinding)std::reverse(face.begin(),face.end());cornerIds.insert(cornerIds.end(),face.begin(),face.end());}}
            else{faceIds=identity.polygons;cornerIds=identity.corners;}
            props.push_back(externalText("iiActorObjectSourcePolygonIds",object3d_detail::externalIds(faceIds)));props.push_back(externalText("iiActorObjectSourceCornerLayouts",object3d_detail::externalIds(cornerIds)));
            std::vector<std::pair<std::string,std::string>> names;for(const auto &m:identity.morphs)names.push_back({m.source,m.source});props.push_back(externalText("iiActorObjectSourceMorphs",object3d_detail::externalMorphs(names)));
        }
        objects.children.push_back(std::move(modelNode));
        connect(follower?geometryIds[leader]:geometry, meshModel);
        connect(meshModel, 0);
        for(const auto material:materials)connect(material,meshModel);
        poseNode(meshModel, Matrix4{});
        if (asset.hasRig()&&!follower)
        {
            const auto &profile=source?source->skinning:g.skinning;
            const char *method=profile.method==SkinningMethod::DualQuaternion?"DualQuaternion":
                               profile.method==SkinningMethod::Blended?"Blend":"Linear";
            objects.children.push_back(
                {"Deformer",
                 {prop(skin), prop(objectName(mesh.name + "Skin", "Deformer")), prop("Skin")},
                 {value("Version", prop(101)), value("Link_DeformAcuracy", prop(50.0)),
                  value("SkinningType", prop(method))}});
            if(profile.method==SkinningMethod::Blended)
            {
                std::vector<std::int32_t> points;points.reserve(profile.blendWeights.size());
                for(std::size_t i=0;i<profile.blendWeights.size();++i)points.push_back(static_cast<std::int32_t>(i));
                objects.children.back().children.push_back(value("Indexes",array(points)));
                objects.children.back().children.push_back(value("BlendWeights",array(profile.blendWeights)));
            }
            connect(skin, geometry);
            for (std::size_t bone = 0; bone < d.bones.size(); ++bone)
            {
                std::vector<std::int32_t> indices;
                std::vector<double> weights;
                for (std::size_t i = 0; i < count; ++i)
                    for (const auto w : source?source->points[i].weights:g.vertices[i].weights)
                        if (w.bone == bone && w.weight > 0)
                        {
                            indices.push_back(static_cast<std::int32_t>(i));
                            weights.push_back(w.weight);
                        }
                if (indices.empty())
                    continue;
                const auto cluster = next++;
                objects.children.push_back(
                    {"Deformer",
                     {prop(cluster), prop(objectName(d.bones[bone].name + "Cluster", "SubDeformer")),
                      prop("Cluster")},
                     {value("Version", prop(100)),
                      {"UserData", {prop(""), prop("")}, {}},
                      value("Indexes", array(indices)),
                      value("Weights", array(weights)),
                      value("Transform", array(asset.skeleton()->inverseBindMatrices()[bone].elements())),
                      value("TransformLink", array(worlds[bone].elements()))}});
                connect(cluster, skin);
                connect(bones[bone], cluster);
            }
        }
        std::vector<TopologyMorph> morphs;
        std::vector<std::size_t> projectedRecords;
        if(!external.document.empty())
        {
            for(auto &group:object3d_detail::logicalMorphGroups(mesh))
            {projectedRecords.push_back(group.recordCount);morphs.push_back(std::move(group.morph));}
        }
        else morphs=morphsFor(mesh);
        if(follower)
        {
            const auto &first=d.meshes[leader];auto previous=morphsFor(first);
            accountShared(morphs.size());
            for(const auto &m:morphs)
            {
                accountShared(m.deltas.size());if(m.normalDeltas)accountShared(m.normalDeltas->size());
                if(!progressive->consumedTargets[leader].contains(m.name))
                    throw std::invalid_argument("Shared progressive FBX geometry cannot preserve independent ordinary morph controls");
            }
            if(morphs!=previous||mesh.morphDefaults!=first.morphDefaults)
                throw std::invalid_argument("Shared progressive FBX meshes have different morph shapes or defaults");
            continue;
        }
        if (!morphs.empty())
        {
            std::map<std::string_view,double,std::less<>> defaults;
            for(const auto &entry:mesh.morphDefaults)defaults.emplace(entry.name,entry.value);
            const auto blend = next++;
            objects.children.push_back(
                {"Deformer",
                 {prop(blend), prop(objectName(mesh.name + "Shapes", "Deformer")), prop("BlendShape")},
                 {value("Version", prop(100))}});
            connect(blend, geometry);
            std::set<std::string> usedNames;
            if(progressive)for(const auto *channel:progressive->channels[meshIndex])usedNames.insert(channel->name);
            struct MorphPayload final
            {
                std::vector<std::int32_t> indices;
                std::vector<double> deltas,normalDeltas;
                Bytes normalMetadata;
                bool exactNormals=true;
            };
            const auto encodeMorph=[&](const TopologyMorph &m,bool progressiveShape)
            {
                MorphPayload result;std::map<std::uint32_t,Vector3> positions,pointNormals;
                if(progressiveShape)
                {
                    constexpr std::size_t limit=128'000'000;
                    const auto account=[&](std::size_t count)
                    {if(count>limit-progressiveShapeWork)throw std::length_error("Progressive FBX shape processing budget");progressiveShapeWork+=count;};
                    account(m.deltas.size());
                    if(m.normalDeltas){account(m.normalDeltas->size());if(source)account(source->layout->definition().layouts.size());}
                }
                for(const auto &v:m.deltas)positions.emplace(v.point,v.position);
                if(m.normalDeltas)
                {
                    std::map<std::uint32_t,Vector3> layoutNormals;
                    for(const auto &v:*m.normalDeltas)layoutNormals.emplace(v.layout,v.normal);
                    if(source)
                    {
                        const auto &layouts=source->layout->definition().layouts;
                        for(std::uint32_t i=0;i<layouts.size();++i)
                        {
                            const auto found=layoutNormals.find(i);auto value=found==layoutNormals.end()?Vector3{}:found->second;
                            const auto normalId=layouts[i].normal;
                            if(normalId!=NoTopologyIndex)
                            {
                                const auto base=source->normals.at(normalId);const auto length=std::hypot(base.x,base.y,base.z);const auto scale=length>0?length:1;
                                value={value.x*scale,value.y*scale,value.z*scale};
                            }
                            const auto [it,added]=pointNormals.emplace(layouts[i].position,value);
                            if(!added&&(std::abs(it->second.x-value.x)>1e-10||std::abs(it->second.y-value.y)>1e-10||std::abs(it->second.z-value.z)>1e-10))result.exactNormals=false;
                        }
                        for(const auto &v:*m.normalDeltas)positions.try_emplace(layouts[v.layout].position,Vector3{});
                    }
                    else pointNormals=std::move(layoutNormals);
                    if(!result.exactNormals&&(progressiveShape||!options.allowMorphNormalMetadata))
                        throw std::invalid_argument(progressiveShape?
                            "Progressive FBX cannot preserve distinct per-layout morph normals":"FBX control-point normals cannot encode distinct layout morph normals; explicitly enable allowMorphNormalMetadata to retain the exact SDK values");
                    for(const auto &[point,value]:pointNormals)if(value!=Vector3{})positions.try_emplace(point,Vector3{});
                }
                for(const auto &[point,value]:positions)
                {
                    result.indices.push_back(static_cast<std::int32_t>(point));xyz(result.deltas,value);
                    if(m.normalDeltas)xyz(result.normalDeltas,pointNormals.contains(point)?pointNormals.at(point):Vector3{});
                }
                if(!progressiveShape&&m.normalDeltas)for(const auto &v:*m.normalDeltas)
                {
                    integer(result.normalMetadata,v.layout,4);
                    for(const auto c:{v.normal.x,v.normal.y,v.normal.z})integer(result.normalMetadata,std::bit_cast<std::uint64_t>(c),8);
                }
                return result;
            };
            const auto makeShape=[&](const TopologyMorph &m,const MorphPayload &payload,std::string_view name,std::int64_t id)
            {
                Node result{"Geometry",{prop(id),prop(objectName(name,"Geometry")),prop("Shape")},
                    {value("Version",prop(100)),value("Indexes",array(payload.indices)),value("Vertices",array(payload.deltas))}};
                if(m.normalDeltas)result.children.push_back(value("Normals",array(payload.normalDeltas)));
                return result;
            };
            for (std::size_t morphIndex=0;morphIndex<morphs.size();++morphIndex)
            {
                const auto &m=morphs[morphIndex];
                if(progressive&&progressive->consumedTargets[meshIndex].contains(m.name))continue;
                auto name=m.name;for(std::size_t suffix=1;!usedNames.insert(name).second;++suffix)name=m.name+"_Source"+std::to_string(suffix);
                const auto channel = next++, shape = next++;
                if(!requestedTakes.empty())
                {
                    const auto logical=logicalMorphIndices.find(m.name);
                    if(logical==logicalMorphIndices.end())throw std::logic_error("FBX morph lacks its logical target");
                    morphChannels[{meshIndex,logical->second}].push_back(channel);
                }
                const auto payload=encodeMorph(m,false);
                const auto foundDefault=defaults.find(m.name);
                const auto defaultPercent=foundDefault==defaults.end()?0.:100.*foundDefault->second;
                if(!std::isfinite(defaultPercent))throw std::overflow_error("FBX default morph percentage overflow");
                objects.children.push_back(
                    {"Deformer",
                     {prop(channel), prop(objectName(name, "SubDeformer")), prop("BlendShapeChannel")},
                     {value("Version", prop(100)), value("DeformPercent", prop(defaultPercent)),
                      value("FullWeights", array(std::array<double, 1>{100.0})),
                      value("iiSourceName",prop(m.name)),value("iiSourceTarget",prop(static_cast<std::int64_t>(m.sourceTarget))),
                      value("iiNormalMode",prop(m.normalDeltas?1:0)),value("iiNormalProjectionExact",prop(payload.exactNormals?1:0)),
                      value("iiLayoutNormals",blob(payload.normalMetadata))}});
                if(!projectedRecords.empty()&&projectedRecords[morphIndex]>1)
                    objects.children.back().children.push_back(value("iiActorObjectMergedMorphRecords",prop(static_cast<std::int64_t>(projectedRecords[morphIndex]))));
                objects.children.push_back(makeShape(m,payload,name,shape));
                connect(shape, channel);
                connect(channel, blend);
            }
            std::map<std::string_view,const TopologyMorph *,std::less<>> progressiveMorphs;
            if(progressive)for(const auto &m:morphs)
                if(progressive->consumedTargets[meshIndex].contains(m.name))progressiveMorphs.emplace(m.name,&m);
            if(progressive)for(const auto *program:progressive->channels[meshIndex])
            {
                const auto channel=next++;std::vector<double> weights;weights.reserve(program->keys.size());
                for(const auto &key:program->keys)weights.push_back(100.*key.fullWeight);
                objects.children.push_back({"Deformer",
                    {prop(channel),prop(objectName(program->name,"SubDeformer")),prop("BlendShapeChannel")},
                    {value("Version",prop(100)),value("DeformPercent",prop(100.*program->defaultValue)),
                     value("FullWeights",array(weights)),value("iiActorObjectProgressive",prop(1))}});
                if(!requestedTakes.empty())progressiveChannels[object3DProgressiveControlCurveName(program->name)].push_back(channel);
                connect(channel,blend);
                for(const auto &key:program->keys)
                {
                    const auto found=progressiveMorphs.find(key.target);
                    if(found==progressiveMorphs.end())throw std::logic_error("Progressive FBX target binding changed");
                    auto morph=*found->second;
                    if(source&&!morph.normalDeltas)
                    {
                        constexpr std::size_t limit=128'000'000;
                        const auto account=[&](std::size_t count)
                        {if(count>limit-progressiveShapeWork)throw std::length_error("Progressive FBX derived normal budget");progressiveShapeWork+=count;};
                        account(source->points.size());account(source->layout->definition().layouts.size());
                        account(source->layout->triangles().size());
                        morph.normalDeltas=topologyMorphNormalDeltas(*source,morph);
                    }
                    const auto payload=encodeMorph(morph,true);const auto shape=next++;
                    objects.children.push_back(makeShape(morph,payload,key.sourceName.empty()?key.target:key.sourceName,shape));
                    connect(shape,channel);
                }
            }
        }
    }
    if (asset.hasRig())
        objects.children.push_back(std::move(pose));
    Node takes{"Takes", {}, {value("Current",prop(""))}};
    if(!requestedTakes.empty())
    {
        if(!animationOptions)throw std::logic_error("Missing FBX animation export options");
        takes.children={value("Current",prop(requestedTakes.front().name))};
        std::size_t remainingSamples=animationOptions->maximumSamples;
        for(std::size_t takeIndex=0;takeIndex<requestedTakes.size();++takeIndex)
        {
            const auto &take=requestedTakes[takeIndex];
            if(!take.clip||remainingSamples==0)throw std::length_error("FBX animation sample budget exceeded");
            auto perTake=*animationOptions;perTake.indexedBindings=take.morphBindings;perTake.maximumSamples=remainingSamples;
            const auto animation=object3d_detail::planFbxAnimation(asset,*take.clip,perTake,morphChannels,
                                                                   progressiveChannels,progressive!=nullptr);
            if(animation.sampleCount>remainingSamples)throw std::logic_error("FBX animation sample budget accounting");
            remainingSamples-=animation.sampleCount;
            const auto origin=takeOriginTick(take.sourceStartSeconds);validateTakeTimes(animation,origin);
            const auto end=origin+animation.duration;
            const auto stack=next++, layer=next++;
            const auto pTime=[](std::string_view name,std::int64_t tick)
            {return Node{"P",{prop(name),prop("KTime"),prop("Time"),prop(""),prop(tick)}, {}};};
            objects.children.push_back({"AnimationStack",{prop(stack),prop(objectName(take.name,"AnimStack")),prop("")},
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
                    connect(node,layer);connectProperty(node,bones.at(track.bone),properties[kind]);
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
            const auto filename=requestedTakes.size()==1&&take.name=="Take"?
                std::string("Take.tak"):"Take_"+std::to_string(takeIndex)+".tak";
            takes.children.push_back({"Take",{prop(take.name)},{value("FileName",prop(filename)),
                    {"LocalTime",{prop(origin),prop(end)},{}},
                    {"ReferenceTime",{prop(origin),prop(end)},{}}}});
        }
    }
    Node header{"FBXHeaderExtension",
                {},
                {value("FBXHeaderVersion", prop(1003)),
                 value("FBXVersion", prop(7400)),
                 value("EncryptionType", prop(0)),
                 {"CreationTimeStamp",
                  {},
                  {value("Version", prop(1000)), value("Year", prop(1970)), value("Month", prop(1)),
                   value("Day", prop(1)), value("Hour", prop(0)), value("Minute", prop(0)),
                   value("Second", prop(0)), value("Millisecond", prop(0))}},
                 value("Creator", prop("iiActorObject Object3DEngine"))}};
    Node settings{"GlobalSettings",
                  {},
                  {value("Version", prop(1000)),
                   {"Properties70",
                    {},
                    {pInt("UpAxis", 1), pInt("UpAxisSign", 1), pInt("FrontAxis", 2),
                     pInt("FrontAxisSign", 1), pInt("CoordAxis", 0), pInt("CoordAxisSign", 1),
                     pNumber("UnitScaleFactor", 100), pNumber("OriginalUnitScaleFactor", 100)}}}};
    Node definitions{"Definitions",
                     {},
                     {value("Version", prop(100)),
                      value("Count", prop(static_cast<std::int32_t>(objects.children.size())))}};
    Node documents{"Documents",
                   {},
                   {value("Count", prop(1)),
                    {"Document",
                     {prop(std::int64_t{1}), prop("Scene"), prop("Scene")},
                     {{"Properties70", {}, {}}, value("RootNode", prop(std::int64_t{0}))}}}};
    const std::string magic("Kaydara FBX Binary  \0\x1a\0", 23);
    Bytes out(magic.begin(), magic.end());
    integer(out, 7400, 4);
    for (const auto &n : std::array{header, settings, documents, Node{"References", {}, {}}, definitions,
                                    objects, connections, takes})
        encode(out, n);
    out.resize(out.size() + 13, 0);
    // Standard binary footer. Contents are structural markers, not asset or application code.
    const std::uint8_t footer[]{0xfa, 0xbc, 0xab, 0x09, 0xd0, 0xc8, 0xd4, 0x66,
                                0xb1, 0x76, 0xfb, 0x83, 0x1c, 0xf7, 0x26, 0x7e};
    out.insert(out.end(), std::begin(footer), std::end(footer));
    while (out.size() % 16)
        out.push_back(0);
    integer(out, 7400, 4);
    out.resize(out.size() + 120, 0);
    const std::uint8_t end[]{0xf8, 0x5a, 0x8c, 0x6a, 0xde, 0xf5, 0xd9, 0x7e,
                             0xec, 0xe9, 0x0c, 0xe3, 0x75, 0x8f, 0x29, 0x0b};
    out.insert(out.end(), std::begin(end), std::end(end));
    return out;
}
} // namespace iiActorObject
