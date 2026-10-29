#include "Object3DGlbPointIdentity.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace iiActorObject::object3d_detail
{
namespace
{
constexpr std::array<std::uint8_t,8> magic{'i','i','G','L','P','T',0,0};
constexpr std::size_t maximumSidecarBytes=256U*1024U*1024U;
void append(std::vector<std::uint8_t> &out,std::uint64_t value,unsigned bytes)
{
    if(out.size()>maximumSidecarBytes||bytes>maximumSidecarBytes-out.size())
        throw std::length_error("GLB point identity byte budget");
    for(unsigned i=0;i<bytes;++i)out.push_back(static_cast<std::uint8_t>(value>>(8*i)));
}
void scalar(std::vector<std::uint8_t> &out,double value)
{
    if(!std::isfinite(value))throw std::invalid_argument("Nonfinite GLB point identity value");
    append(out,std::bit_cast<std::uint64_t>(value),8);
}
void vector(std::vector<std::uint8_t> &out,Vector3 value)
{scalar(out,value.x);scalar(out,value.y);scalar(out,value.z);}
void text(std::vector<std::uint8_t> &out,std::string_view value)
{
    if(value.empty()||value.size()>1024||value.find('\0')!=value.npos)
        throw std::invalid_argument("Invalid GLB point identity name");
    append(out,value.size(),4);
    if(value.size()>maximumSidecarBytes-out.size())throw std::length_error("GLB point identity byte budget");
    out.insert(out.end(),value.begin(),value.end());
}
class Reader final
{
public:
    explicit Reader(std::span<const std::uint8_t> bytes):bytes_(bytes){}
    std::uint64_t integer(unsigned count)
    {
        if(count>bytes_.size()-offset_)throw std::invalid_argument("Truncated GLB point identity");
        std::uint64_t value=0;for(unsigned i=0;i<count;++i)value|=std::uint64_t(bytes_[offset_++])<<(8*i);return value;
    }
    double scalar()
    {
        const auto value=std::bit_cast<double>(integer(8));
        if(!std::isfinite(value))throw std::invalid_argument("Nonfinite GLB point identity value");return value;
    }
    Vector3 vector(){return {scalar(),scalar(),scalar()};}
    std::string text()
    {
        const auto count=integer(4);
        if(!count||count>1024||count>remaining())throw std::invalid_argument("Invalid GLB point identity name");
        std::string value(reinterpret_cast<const char*>(bytes_.data()+offset_),static_cast<std::size_t>(count));
        offset_+=static_cast<std::size_t>(count);
        if(value.find('\0')!=value.npos)throw std::invalid_argument("Invalid GLB point identity name");return value;
    }
    std::size_t remaining() const noexcept{return bytes_.size()-offset_;}
    bool finished() const noexcept{return offset_==bytes_.size();}
private:
    std::span<const std::uint8_t> bytes_;std::size_t offset_=0;
};
} // namespace

std::vector<std::uint8_t> encodeGlbPointIdentity(const Object3DMesh &mesh,std::span<const Bone> bones)
{
    if(!mesh.topology||!mesh.topology->layout)throw std::invalid_argument("GLB point identity requires source topology");
    const auto &topology=*mesh.topology;const auto &definition=topology.layout->definition();
    if(topology.points.size()>=NoTopologyIndex||mesh.geometry.morphTargets.size()>=NoTopologyIndex||bones.size()>=NoTopologyIndex)
        throw std::length_error("GLB point identity count limit");
    std::vector<bool> visible(topology.points.size());
    for(const auto layout:definition.renderToLayout)visible.at(definition.layouts.at(layout).position)=true;
    const auto unused=static_cast<std::uint32_t>(std::count(visible.begin(),visible.end(),false));
    std::map<std::string_view,std::uint32_t,std::less<>> targets;
    for(std::size_t i=0;i<mesh.geometry.morphTargets.size();++i)
        targets.emplace(mesh.geometry.morphTargets[i].name,static_cast<std::uint32_t>(i));
    std::size_t morphCount=0;
    for(const auto &morph:topology.morphTargets)
    {
        if(!targets.contains(morph.name))throw std::invalid_argument("Missing GLB point identity morph channel");
        for(const auto &delta:morph.deltas)if(!visible.at(delta.point))
        {if(morphCount==16'000'000)throw std::length_error("GLB point identity morph budget");++morphCount;}
    }
    const auto emptyWeights=!bones.empty()?static_cast<std::uint32_t>(std::count_if(topology.points.begin(),topology.points.end(),
        [](const auto &point){return point.weights.empty();})):0;
    std::vector<std::uint8_t> result(magic.begin(),magic.end());
    append(result,1,4);append(result,topology.points.size(),4);append(result,targets.size(),4);
    append(result,unused,4);append(result,morphCount,4);append(result,emptyWeights,4);append(result,bones.size(),4);
    for(const auto &target:mesh.geometry.morphTargets)text(result,target.name);
    for(const auto &bone:bones)text(result,bone.name);
    for(std::size_t i=0;i<topology.points.size();++i)
    {
        if(visible[i])continue;const auto &point=topology.points[i];
        if(point.weights.size()>=NoTopologyIndex)throw std::length_error("GLB point identity influence limit");
        append(result,i,4);vector(result,point.position);append(result,point.weights.size(),4);
        for(const auto &weight:point.weights){append(result,weight.bone,4);scalar(result,weight.weight);}
    }
    for(const auto &morph:topology.morphTargets)
        for(const auto &delta:morph.deltas)if(!visible.at(delta.point))
        {append(result,delta.point,4);append(result,targets.at(morph.name),4);vector(result,delta.position);}
    if(!bones.empty())for(std::size_t i=0;i<topology.points.size();++i)
        if(topology.points[i].weights.empty())append(result,i,4);
    return result;
}

GlbPointIdentity decodeGlbPointIdentity(std::span<const std::uint8_t> bytes,
    std::size_t maximumPoints,std::size_t maximumTargets,std::size_t maximumMorphs,
    std::size_t maximumInfluences,std::size_t maximumBones)
{
    if(bytes.size()<36||bytes.size()>maximumSidecarBytes||!std::equal(magic.begin(),magic.end(),bytes.begin()))
        throw std::invalid_argument("Invalid GLB point identity envelope");
    Reader reader(bytes.subspan(magic.size()));
    if(reader.integer(4)!=1)throw std::invalid_argument("Unknown GLB point identity version");
    GlbPointIdentity result;result.pointCount=static_cast<std::uint32_t>(reader.integer(4));
    result.targetCount=static_cast<std::uint32_t>(reader.integer(4));
    const auto unused=reader.integer(4),morphs=reader.integer(4),emptyWeights=reader.integer(4),bones=reader.integer(4);
    if(!result.pointCount||result.pointCount>maximumPoints||result.pointCount>=NoTopologyIndex||
       result.targetCount>maximumTargets||bones>maximumBones||unused>result.pointCount||morphs>maximumMorphs||
       emptyWeights>result.pointCount)
        throw std::length_error("GLB point identity count limit");
    std::set<std::string> names;
    for(std::size_t i=0;i<result.targetCount;++i)
    {auto value=reader.text();if(!names.insert(value).second)throw std::invalid_argument("Duplicate GLB point identity target");result.targetNames.push_back(std::move(value));}
    names.clear();
    for(std::size_t i=0;i<bones;++i)
    {auto value=reader.text();if(!names.insert(value).second)throw std::invalid_argument("Duplicate GLB point identity bone");result.boneNames.push_back(std::move(value));}
    if(unused>reader.remaining()/32)throw std::invalid_argument("Truncated GLB point identity records");
    std::vector<bool> seen(result.pointCount);std::size_t totalWeights=0;
    result.unused.reserve(static_cast<std::size_t>(unused));
    for(std::size_t i=0;i<unused;++i)
    {
        GlbUnusedPoint point;point.id=static_cast<std::uint32_t>(reader.integer(4));point.position=reader.vector();
        const auto count=reader.integer(4);
        if(point.id>=result.pointCount||seen[point.id]||count>maximumInfluences||count>16'000'000-totalWeights)
            throw std::invalid_argument("Invalid GLB unused point identity");
        seen[point.id]=true;totalWeights+=static_cast<std::size_t>(count);
        std::set<BoneIndex> bones;double sum=0;
        for(std::size_t j=0;j<count;++j)
        {
            const auto bone=reader.integer(4);const auto weight=reader.scalar();
            if(bone>=NoBone||weight<0||weight>1||!bones.insert(static_cast<BoneIndex>(bone)).second)
                throw std::invalid_argument("Invalid GLB unused point weight");
            point.weights.push_back({static_cast<BoneIndex>(bone),weight});sum+=weight;
        }
        if(count&&(!std::isfinite(sum)||std::abs(sum-1)>1e-6))
            throw std::invalid_argument("Invalid GLB unused point weight sum");
        result.unused.push_back(std::move(point));
    }
    if(morphs>reader.remaining()/32)throw std::invalid_argument("Truncated GLB point identity morphs");
    result.morphs.reserve(static_cast<std::size_t>(morphs));
    for(std::size_t i=0;i<morphs;++i)
    {
        GlbUnusedPointMorph morph;morph.point=static_cast<std::uint32_t>(reader.integer(4));
        morph.target=static_cast<std::uint32_t>(reader.integer(4));morph.delta=reader.vector();
        if(morph.point>=result.pointCount||!seen[morph.point]||morph.target>=result.targetCount)
            throw std::invalid_argument("Invalid GLB unused point morph");
        result.morphs.push_back(morph);
    }
    if(emptyWeights>reader.remaining()/4)throw std::invalid_argument("Truncated GLB empty weight identities");
    std::vector<bool> emptySeen(result.pointCount);result.emptyWeights.reserve(static_cast<std::size_t>(emptyWeights));
    for(std::size_t i=0;i<emptyWeights;++i)
    {
        const auto id=static_cast<std::uint32_t>(reader.integer(4));
        if(id>=result.pointCount||emptySeen[id])throw std::invalid_argument("Invalid GLB empty weight identity");
        emptySeen[id]=true;result.emptyWeights.push_back(id);
    }
    if(!reader.finished())throw std::invalid_argument("Trailing GLB point identity data");
    return result;
}
} // namespace iiActorObject::object3d_detail
