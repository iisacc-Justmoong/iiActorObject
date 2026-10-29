#include "Object3DBvh.h"
#include "Object3DNumberParsing.h"
#include "CharacterMath.h"
#include <charconv>
#include <cctype>
#include <numbers>
#include <set>
#include <type_traits>

namespace iiActorObject
{
namespace
{
using namespace character_detail;
class Tokens final
{
  public:
    explicit Tokens(std::string_view text):text_(text){}
    std::string_view next()
    {
        while(position_<text_.size()&&std::isspace(static_cast<unsigned char>(text_[position_])))++position_;
        if(position_==text_.size())return {};
        const auto first=position_++;
        if(text_[first]!='{'&&text_[first]!='}')while(position_<text_.size()&&!std::isspace(static_cast<unsigned char>(text_[position_]))&&text_[position_]!='{'&&text_[position_]!='}')++position_;
        return text_.substr(first,position_-first);
    }
    void expect(std::string_view word){if(next()!=word)throw std::invalid_argument("Invalid BVH token; expected "+std::string(word));}
    template<class T>T number()
    {
        auto token=next();if(!token.empty()&&token.front()=='+'){token.remove_prefix(1);if(!token.empty()&&(token.front()=='+'||token.front()=='-'))throw std::invalid_argument("Invalid BVH numeric sign");}if(token.empty())throw std::invalid_argument("Missing BVH number");T value{};
        if(!object3d_detail::readNumber(token,value))throw std::invalid_argument("Invalid BVH number");return value;
    }
    Vector3 vector(){return {number<double>(),number<double>(),number<double>()};}
  private:
    std::string_view text_;
    std::size_t position_=0;
};
constexpr std::array<std::string_view,6> channelNames{"Xposition","Yposition","Zposition","Xrotation","Yrotation","Zrotation"};
}
BvhMotion parseObject3DBvh(std::string_view text,const BvhImportOptions &options)
{
    using namespace character_detail;
    if(text.empty()||text.size()>options.maximumBytes||!std::isfinite(options.scale)||options.scale<=0||
       !options.maximumJoints||options.maximumJoints>65535||!options.maximumFrames||!options.maximumSamples)
        throw std::invalid_argument("Invalid BVH input or limits");
    if(text.starts_with("\xef\xbb\xbf"))text.remove_prefix(3);
    BvhMotion result;result.scale_=options.scale;result.basis_=options.basis.normalized();const auto basis=Transform{{},result.basis_}.matrix();Tokens tokens(text);tokens.expect("HIERARCHY");tokens.expect("ROOT");
    std::set<std::string> names;
    const auto joint=[&](auto &&self,BoneIndex parent,std::size_t depth)->void
    {
        if(depth>256||result.joints_.size()>=options.maximumJoints)throw std::length_error("BVH joint or depth limit");
        const std::string name(tokens.next());if(name.empty()||name.size()>1024||name.find('\0')!=name.npos||!names.insert(name).second)throw std::invalid_argument("Invalid or duplicate BVH joint name");
        const auto id=static_cast<BoneIndex>(result.joints_.size());result.joints_.push_back({name,parent,{},{},{},result.channels_});
        tokens.expect("{");tokens.expect("OFFSET");const auto offset=tokens.vector();result.joints_[id].offset=basis.transformVector(mul(offset,result.scale_));
        if(!finite(result.joints_[id].offset))throw std::invalid_argument("BVH offset overflow");
        tokens.expect("CHANNELS");const auto channels=tokens.number<std::size_t>();if(channels>6)throw std::invalid_argument("BVH channel count");
        std::set<BvhChannel> seen;
        for(std::size_t i=0;i<channels;++i)
        {
            const auto channel=tokens.next();const auto found=std::find(channelNames.begin(),channelNames.end(),channel);if(found==channelNames.end())throw std::invalid_argument("Unknown BVH channel");
            const auto value=static_cast<BvhChannel>(found-channelNames.begin());if(!seen.insert(value).second)throw std::invalid_argument("Duplicate BVH channel");result.joints_[id].channels.push_back(value);
        }
        result.channels_+=channels;
        while(true)
        {
            const auto token=tokens.next();if(token=="}")break;
            if(token=="JOINT")self(self,id,depth+1);
            else if(token=="End")
            {
                if(result.joints_[id].endSite)throw std::invalid_argument("Duplicate BVH End Site");
                tokens.expect("Site");tokens.expect("{");tokens.expect("OFFSET");const auto end=tokens.vector();result.joints_[id].endSite=basis.transformVector(mul(end,result.scale_));tokens.expect("}");
                if(!finite(*result.joints_[id].endSite))throw std::invalid_argument("BVH endpoint overflow");
            }
            else throw std::invalid_argument("Invalid BVH joint body");
        }
    };
    joint(joint,NoBone,0);tokens.expect("MOTION");tokens.expect("Frames:");result.frames_=tokens.number<std::size_t>();tokens.expect("Frame");tokens.expect("Time:");result.interval_=tokens.number<double>();
    if(!result.frames_||result.frames_>options.maximumFrames||!result.channels_||result.frames_>options.maximumSamples/result.channels_||result.frames_>options.maximumSamples/result.joints_.size()||result.interval_<=0||!std::isfinite(result.interval_*result.frames_))throw std::length_error("BVH frame, sample or interval limit");
    result.samples_.reserve(result.frames_*result.channels_);for(std::size_t i=0;i<result.frames_*result.channels_;++i)result.samples_.push_back(tokens.number<double>());
    if(!tokens.next().empty())throw std::invalid_argument("Trailing BVH motion data");
    for(std::size_t i=0;i<result.frames_;++i)(void)result.frame(i);return result;
}
std::span<const BvhJoint> BvhMotion::joints() const noexcept{return joints_;}
std::size_t BvhMotion::frameCount() const noexcept{return frames_;}
double BvhMotion::frameTime() const noexcept{return interval_;}
std::vector<Transform> BvhMotion::frame(std::size_t index) const
{
    using namespace character_detail;if(index>=frames_)throw std::out_of_range("BVH frame index");
    std::vector<Transform> result;result.reserve(joints_.size());const auto basis=Transform{{},basis_}.matrix();
    for(const auto &joint:joints_)
    {
        Vector3 translation{};Quaternion rotation;auto column=index*channels_+joint.firstChannel;
        for(const auto channel:joint.channels)
        {
            const auto value=samples_[column++];const auto axis=static_cast<unsigned>(channel)%3;
            constexpr std::array<Vector3,3> axes{{{1,0,0},{0,1,0},{0,0,1}}};const auto direction=axes[axis];
            if(static_cast<unsigned>(channel)<3)translation=add(translation,mul(direction,value*scale_));
            else rotation=product(rotation,Quaternion::fromAxisAngle(direction,std::remainder(value,360.)*std::numbers::pi/180));
        }
        translation=add(joint.offset,basis.transformVector(translation));rotation=product(product(basis_,rotation),conjugate(basis_));
        if(!finite(translation))throw std::invalid_argument("BVH translation overflow");result.push_back({translation,rotation,{1,1,1}});
    }
    return result;
}
std::vector<Quaternion> BvhMotion::worldRotations(std::size_t index) const
{
    using namespace character_detail;const auto local=frame(index);std::vector<Quaternion> result(joints_.size());
    for(std::size_t i=0;i<joints_.size();++i)result[i]=joints_[i].parent==NoBone?local[i].rotation:product(result[joints_[i].parent],local[i].rotation);return result;
}
} // namespace iiActorObject
