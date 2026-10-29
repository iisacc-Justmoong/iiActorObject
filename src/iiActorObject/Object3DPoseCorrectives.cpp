#include "Object3DPoseCorrectives.h"
#include "Object3DPoseCorrectivesBackend.h"
#include "CharacterMath.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <map>
#include <numbers>
#include <set>
#include <stdexcept>

namespace iiActorObject
{
namespace object3d_detail
{
struct PoseCorrectiveRigAccess final
{
    static bool isCorrective(const Object3DRigSession &rig)noexcept{return isPoseCorrectiveDriver(*rig.driver_);}
    static const Object3DRigDriver &driver(const Object3DRigSession &rig)noexcept{return *rig.driver_;}
};
}
namespace
{
using namespace character_detail;
constexpr std::size_t MaximumSamples=4096,MaximumConditions=65'536,MaximumJoints=4096,MaximumNameBytes=1024;
constexpr std::array<char,8> Magic{'i','i','A','O','P','C','R',0};
constexpr std::string_view SourceFormat="iiActorObject.poseCorrective.1";

void name(std::string_view value)
{
    if(value.empty()||value.size()>MaximumNameBytes||value.find('\0')!=value.npos)
        throw std::invalid_argument("Invalid pose corrective name");
}
Quaternion unitQuaternion(Quaternion value)
{
    const auto result=value.normalized();
    if(std::max({std::abs(value.x-result.x),std::abs(value.y-result.y),
                 std::abs(value.z-result.z),std::abs(value.w-result.w)})>1e-6)
        throw std::invalid_argument("Pose corrective rotation must be normalized");
    return result;
}
std::uint32_t crc(std::span<const std::byte> bytes)
{
    std::uint32_t value=0xffffffffu;
    for(const auto byte:bytes)
    {
        value^=std::to_integer<unsigned>(byte);
        for(unsigned bit=0;bit<8;++bit)value=(value>>1)^((value&1)?0xedb88320u:0u);
    }
    return ~value;
}
class Writer final
{
  public:
    explicit Writer(std::size_t maximum):maximum_(maximum){}
    void integer(std::uint64_t value,unsigned size)
    {
        if(size>maximum_-data_.size())throw std::length_error("Pose corrective byte budget");
        for(unsigned i=0;i<size;++i)data_.push_back(static_cast<std::byte>((value>>(8*i))&255));
    }
    void string(std::string_view value)
    {
        integer(value.size(),4);
        if(value.size()>maximum_-data_.size())throw std::length_error("Pose corrective byte budget");
        for(const auto character:value)data_.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
    }
    [[nodiscard]] std::vector<std::byte> take(){return std::move(data_);}
  private:
    std::size_t maximum_;std::vector<std::byte> data_;
};
class Reader final
{
  public:
    explicit Reader(std::span<const std::byte> data):data_(data){}
    std::uint64_t integer(unsigned size)
    {
        if(size>data_.size()-offset_)throw std::invalid_argument("Truncated pose corrective program");
        std::uint64_t value=0;
        for(unsigned i=0;i<size;++i)value|=std::uint64_t(std::to_integer<unsigned>(data_[offset_++]))<<(8*i);
        return value;
    }
    std::string string()
    {
        const auto size=integer(4);
        if(size>MaximumNameBytes||size>data_.size()-offset_)throw std::invalid_argument("Invalid pose corrective name size");
        std::string value(reinterpret_cast<const char*>(data_.data()+offset_),static_cast<std::size_t>(size));
        offset_+=static_cast<std::size_t>(size);return value;
    }
    void finish()const{if(offset_!=data_.size())throw std::invalid_argument("Trailing pose corrective program data");}
  private:
    std::span<const std::byte> data_;std::size_t offset_=0;
};
using object3d_detail::PoseCorrectiveBoundSample;

class PoseCorrectiveDriver final:public Object3DRigDriver
{
  public:
    explicit PoseCorrectiveDriver(Object3DPoseCorrectiveProgram program):program_(std::move(program))
    {
        validateObject3DPoseCorrectiveProgram(program_);definition_.sourceFormat=SourceFormat;
        definition_.drivenJoints.emplace();std::map<std::string,std::size_t,std::less<>> joints,morphs;
        for(const auto &sample:program_.samples)
        {
            auto [morph,inserted]=morphs.emplace(sample.morph,morphs.size());
            if(inserted)definition_.blendShapes.push_back(sample.morph);
            PoseCorrectiveBoundSample compiled;compiled.morph=morph->second;compiled.maximumWeight=sample.maximumWeight;
            for(const auto &condition:sample.conditions)
            {
                auto [joint,added]=joints.emplace(condition.joint,joints.size());
                if(added)definition_.joints.push_back(condition.joint);
                compiled.conditions.push_back({joint->second,unitQuaternion(condition.rotation),condition.radiusRadians});
            }
            samples_.push_back(std::move(compiled));
        }
        for(std::size_t joint=0;joint<definition_.joints.size();++joint)
            for(const auto axis:{"x","y","z","w"})
            {
                definition_.rawControls.push_back("joint/"+std::to_string(joint)+"/"+axis);
                definition_.rawControlDefaults.push_back(std::string_view(axis)=="w"?1.:0.);
            }
        definition_.guiControls=definition_.rawControls;definition_.guiControlDefaults=definition_.rawControlDefaults;
    }
    const Object3DRigDefinition &definition()const noexcept override{return definition_;}
    std::span<const PoseCorrectiveBoundSample> boundSamples()const noexcept{return samples_;}
    std::size_t work()const noexcept
    {
        auto total=definition_.joints.size()+samples_.size();
        for(const auto &sample:samples_)total+=sample.conditions.size();
        return total;
    }
    std::unique_ptr<Object3DRigDriver> clone()const override{return std::make_unique<PoseCorrectiveDriver>(*this);}
    std::unique_ptr<Object3DRigDriver> cloneFor(const RigSourceSnapshot &source)const override
    {
        if(!source.asset||!source.asset->hasRig())throw std::invalid_argument("Pose correctives require a rigged core asset");
        std::set<std::string_view> available;
        for(const auto &mesh:source.asset->data().meshes)
            for(const auto &morph:mesh.geometry.morphTargets)available.insert(morph.name);
        for(const auto &channel:definition_.blendShapes)
            if(!available.contains(channel))throw std::invalid_argument("Pose corrective morph is missing from the target asset: "+channel);
        return clone();
    }
    std::vector<std::byte> sourceBytes()const override{return exportObject3DPoseCorrectiveProgram(program_);}
    Object3DRigOutputs evaluate(std::span<const double> controls,RigControlSpace space,std::size_t lod)override
    {
        if((space!=RigControlSpace::Raw&&space!=RigControlSpace::Gui)||lod!=0||controls.size()!=4*definition_.joints.size())
            throw std::invalid_argument("Invalid pose corrective control dimensions");
        std::vector<Quaternion> rotations;rotations.reserve(definition_.joints.size());
        for(std::size_t i=0;i<controls.size();i+=4)
            rotations.push_back(unitQuaternion({controls[i],controls[i+1],controls[i+2],controls[i+3]}));
        Object3DRigOutputs output;output.joints.resize(definition_.joints.size());
        output.blendShapes.resize(definition_.blendShapes.size());
        for(const auto &sample:samples_)
        {
            auto value=sample.maximumWeight;
            for(const auto &condition:sample.conditions)
            {
                const auto difference=product(conjugate(condition.rotation),rotations[condition.joint]);
                const auto angle=2*std::atan2(std::hypot(difference.x,difference.y,difference.z),std::abs(difference.w));
                const auto t=std::clamp(1-angle/condition.radius,0.,1.);
                value*=t*t*(3-2*t);if(value==0)break;
            }
            output.blendShapes[sample.morph]=std::max(output.blendShapes[sample.morph],value);
        }
        return output;
    }
  private:
    Object3DPoseCorrectiveProgram program_;Object3DRigDefinition definition_;std::vector<PoseCorrectiveBoundSample> samples_;
};
void requireCorrectiveRig(const Object3DRigSession &rig)
{
    if(!object3d_detail::PoseCorrectiveRigAccess::isCorrective(rig))throw std::invalid_argument("Not a pose corrective rig");
}
}
void validateObject3DPoseCorrectiveProgram(const Object3DPoseCorrectiveProgram &program)
{
    if(program.samples.empty()||program.samples.size()>MaximumSamples)throw std::length_error("Pose corrective sample budget");
    std::set<std::string_view> joints,morphs;std::size_t conditions=0;
    for(const auto &sample:program.samples)
    {
        name(sample.morph);morphs.insert(sample.morph);
        if(!std::isfinite(sample.maximumWeight)||sample.maximumWeight<0||sample.maximumWeight>1)
            throw std::invalid_argument("Invalid pose corrective maximum weight");
        if(sample.conditions.empty()||sample.conditions.size()>MaximumConditions-conditions)
            throw std::length_error("Pose corrective condition budget");
        conditions+=sample.conditions.size();std::set<std::string_view> seen;
        for(const auto &condition:sample.conditions)
        {
            name(condition.joint);joints.insert(condition.joint);
            if(!seen.insert(condition.joint).second)throw std::invalid_argument("Duplicate joint in pose corrective sample");
            if(!std::isfinite(condition.radiusRadians)||condition.radiusRadians<=0||condition.radiusRadians>std::numbers::pi)
                throw std::invalid_argument("Invalid pose corrective angular radius");
            (void)unitQuaternion(condition.rotation);
        }
    }
    if(joints.size()>MaximumJoints||morphs.size()>MaximumSamples)throw std::length_error("Pose corrective channel budget");
}
std::vector<std::byte> exportObject3DPoseCorrectiveProgram(const Object3DPoseCorrectiveProgram &program,std::size_t maximumBytes)
{
    validateObject3DPoseCorrectiveProgram(program);if(maximumBytes<24)throw std::length_error("Pose corrective byte budget");
    Writer payload(maximumBytes-24);payload.integer(program.samples.size(),4);
    for(const auto &sample:program.samples)
    {
        payload.string(sample.morph);payload.integer(std::bit_cast<std::uint64_t>(sample.maximumWeight),8);
        payload.integer(sample.conditions.size(),4);
        for(const auto &condition:sample.conditions)
        {
            payload.string(condition.joint);
            for(const auto value:{condition.rotation.x,condition.rotation.y,condition.rotation.z,condition.rotation.w,condition.radiusRadians})
                payload.integer(std::bit_cast<std::uint64_t>(value),8);
        }
    }
    auto body=payload.take();Writer header(24);
    for(const auto character:Magic)header.integer(static_cast<unsigned char>(character),1);
    header.integer(1,4);header.integer(body.size(),8);header.integer(crc(body),4);
    auto result=header.take();result.insert(result.end(),body.begin(),body.end());return result;
}
Object3DPoseCorrectiveProgram importObject3DPoseCorrectiveProgram(std::span<const std::byte> bytes,std::size_t maximumBytes)
{
    if(bytes.size()<24||bytes.size()>maximumBytes||std::memcmp(bytes.data(),Magic.data(),Magic.size())!=0)
        throw std::invalid_argument("Invalid pose corrective program header");
    Reader header(bytes.subspan(8,16));
    if(header.integer(4)!=1||header.integer(8)!=bytes.size()-24||header.integer(4)!=crc(bytes.subspan(24)))
        throw std::invalid_argument("Invalid pose corrective version, size or checksum");
    Reader reader(bytes.subspan(24));const auto count=reader.integer(4);
    if(!count||count>MaximumSamples)throw std::length_error("Pose corrective sample budget");
    Object3DPoseCorrectiveProgram program;program.samples.reserve(static_cast<std::size_t>(count));std::size_t total=0;
    for(std::size_t i=0;i<count;++i)
    {
        PoseCorrectiveSample sample;sample.morph=reader.string();sample.maximumWeight=std::bit_cast<double>(reader.integer(8));
        const auto n=reader.integer(4);if(!n||n>MaximumConditions-total)throw std::length_error("Pose corrective condition budget");
        total+=static_cast<std::size_t>(n);sample.conditions.reserve(static_cast<std::size_t>(n));
        for(std::size_t j=0;j<n;++j)
        {
            PoseCorrectiveCondition condition;condition.joint=reader.string();
            condition.rotation={std::bit_cast<double>(reader.integer(8)),std::bit_cast<double>(reader.integer(8)),
                                std::bit_cast<double>(reader.integer(8)),std::bit_cast<double>(reader.integer(8))};
            condition.radiusRadians=std::bit_cast<double>(reader.integer(8));sample.conditions.push_back(std::move(condition));
        }
        program.samples.push_back(std::move(sample));
    }
    reader.finish();validateObject3DPoseCorrectiveProgram(program);return program;
}
Object3DRigHandle createObject3DPoseCorrectiveRig(const Object3DHandle &object,
    const Object3DPoseCorrectiveProgram &program,std::span<const BoneIndex> jointMapping)
{
    auto driver=object3d_detail::makePoseCorrectiveDriver(program);
    auto bound=driver->cloneFor({&object.get(),jointMapping});
    return object.engine().createRigSession(object,std::move(bound),jointMapping);
}
Object3DRigEvaluation evaluateObject3DPoseCorrectives(const Object3DRigHandle &handle,const Pose &pose)
{
    auto &rig=handle.get();requireCorrectiveRig(rig);const auto &asset=rig.object().get();
    if(pose.skeleton()!=asset.skeleton())throw std::invalid_argument("Pose corrective skeleton identity mismatch");
    std::vector<double> values;values.reserve(rig.jointMapping().size()*4);
    for(const auto bone:rig.jointMapping())
    {
        const auto rotation=product(conjugate(asset.skeleton()->bones()[bone].bindTransform.rotation),pose.locals()[bone].rotation);
        values.insert(values.end(),{rotation.x,rotation.y,rotation.z,rotation.w});
    }
    return rig.evaluate(values);
}
void applyObject3DPoseCorrectives(const Object3DRigHandle &rig,iiCharacter &actor)
{
    const auto result=evaluateObject3DPoseCorrectives(rig,actor.pose());rig.get().applyTo(actor,result);
}
PoseCorrectiveComponent::PoseCorrectiveComponent(Object3DRigHandle rig):ActorComponent("Pose correctives"),rig_(std::move(rig))
{requireCorrectiveRig(rig_.get());}
const Object3DRigHandle &PoseCorrectiveComponent::rig()const noexcept{return rig_;}
std::exception_ptr PoseCorrectiveComponent::error()const noexcept{return error_;}
void PoseCorrectiveComponent::update()
{
    try
    {
        auto *character=dynamic_cast<iiCharacter*>(owner());
        if(!character)throw std::invalid_argument("Pose corrective component requires a character owner");
        applyObject3DPoseCorrectives(rig_,*character);error_=nullptr;
    }
    catch(...){error_=std::current_exception();throw;}
}
void PoseCorrectiveComponent::updateNoexcept()noexcept{try{update();}catch(...) {}}
void PoseCorrectiveComponent::onBeginPlay()noexcept{if(isActive())updateNoexcept();}
void PoseCorrectiveComponent::onTick(double)noexcept{updateNoexcept();}
PoseCorrectiveComponent &attachObject3DPoseCorrectives(iiCharacter &actor,const Object3DRigHandle &rig)
{
    (void)evaluateObject3DPoseCorrectives(rig,actor.pose());
    return actor.createComponent<PoseCorrectiveComponent>(rig);
}
namespace object3d_detail
{
bool isPoseCorrectiveDriver(const Object3DRigDriver &driver)noexcept{return dynamic_cast<const PoseCorrectiveDriver*>(&driver)!=nullptr;}
std::size_t poseCorrectiveRigWork(const Object3DRigSession &rig)
{
    const auto *driver=dynamic_cast<const PoseCorrectiveDriver*>(&PoseCorrectiveRigAccess::driver(rig));
    if(!driver)throw std::invalid_argument("Not a pose corrective rig");
    return driver->work();
}
std::span<const PoseCorrectiveBoundSample> poseCorrectiveRigBoundSamples(const Object3DRigSession &rig)
{
    const auto *driver=dynamic_cast<const PoseCorrectiveDriver*>(&PoseCorrectiveRigAccess::driver(rig));
    if(!driver)throw std::invalid_argument("Not a pose corrective rig");
    return driver->boundSamples();
}
std::unique_ptr<Object3DRigDriver> makePoseCorrectiveDriver(Object3DPoseCorrectiveProgram program)
{return std::make_unique<PoseCorrectiveDriver>(std::move(program));}
}
} // namespace iiActorObject
