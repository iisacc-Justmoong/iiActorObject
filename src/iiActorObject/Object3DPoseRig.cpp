#include "Object3DPoseRig.h"
#include "Object3DPoseRigBackend.h"
#include "CharacterMath.h"
#include <bit>
#include <cstring>
#include <set>
#include <numbers>

namespace iiActorObject
{
namespace
{
using namespace character_detail;
constexpr std::size_t missing=std::numeric_limits<std::size_t>::max();
void names(std::span<const std::string> values)
{
    std::set<std::string_view> seen;for(const auto &name:values)
        if(name.empty()||name.size()>1024||name.find('\0')!=name.npos||!seen.insert(name).second)throw std::invalid_argument("Invalid pose program name");
}
class PoseDriver final:public Object3DRigDriver
{
  public:
    explicit PoseDriver(std::shared_ptr<const Object3DPoseProgram> program):program_(std::move(program))
    {
        definition_.sourceFormat="iiActorObject.poseRig.1";definition_.joints=program_->joints;
        for(const auto &unit:program_->units){definition_.rawControls.push_back(unit.name);definition_.blendShapes.push_back(object3DPoseMorphName(unit.name));}
        definition_.guiControls=definition_.rawControls;
        definition_.drivenJoints.emplace();
    }
    PoseDriver(std::shared_ptr<const Object3DPoseProgram> program,const Object3DData &data,std::span<const BoneIndex> mapping):PoseDriver(std::move(program))
    {
        skeleton_=std::make_shared<Skeleton>(data.bones);if(!mapping.empty()&&mapping.size()!=program_->joints.size())throw std::invalid_argument("Pose rig mapping dimensions");
        boneToJoint_.resize(data.bones.size(),missing);bindWorld_.resize(data.bones.size());
        for(std::size_t i=0;i<program_->joints.size();++i)
        {
            const auto bone=mapping.empty()?skeleton_->findBone(program_->joints[i]):mapping[i];
            if(bone>=data.bones.size()||boneToJoint_[bone]!=missing)throw std::invalid_argument("Missing or duplicate pose rig joint mapping");
            joints_.push_back(bone);boneToJoint_[bone]=i;
        }
        for(const auto bone:skeleton_->evaluationOrder())
        {const auto &b=data.bones[bone];bindWorld_[bone]=b.parent==NoBone?b.bindTransform.rotation:product(bindWorld_[b.parent],b.bindTransform.rotation);}
        std::vector<bool> driven(joints_.size(),false);std::vector<double> values(program_->units.size(),0);
        for(std::size_t unit=0;unit<values.size();++unit)
        {
            values[unit]=1;const auto output=evaluate(values,RigControlSpace::Raw,0);values[unit]=0;
            for(std::size_t joint=0;joint<joints_.size();++joint)
            {const auto q=output.joints[joint].rotation;if(std::max({std::abs(q.x),std::abs(q.y),std::abs(q.z)})>1e-12)driven[joint]=true;}
        }
        for(std::size_t joint=0;joint<driven.size();++joint)if(driven[joint])definition_.drivenJoints->push_back(joint);
    }
    const Object3DRigDefinition &definition()const noexcept override{return definition_;}
    std::unique_ptr<Object3DRigDriver> clone()const override{return std::make_unique<PoseDriver>(*this);}
    std::unique_ptr<Object3DRigDriver> cloneFor(const RigSourceSnapshot &source)const override
    {if(!source.asset)throw std::invalid_argument("Pose rebind requires a source object");return std::make_unique<PoseDriver>(program_,source.asset->data(),source.joints);}
    std::vector<std::byte> sourceBytes()const override{return exportObject3DPoseProgram(*program_);}
    Object3DRigOutputs evaluate(std::span<const double> values,RigControlSpace space,std::size_t lod)override
    {
        if(!skeleton_)throw std::logic_error("Pose program needs a target skeleton binding");
        if((space!=RigControlSpace::Raw&&space!=RigControlSpace::Gui)||lod||values.size()!=program_->units.size())throw std::invalid_argument("Invalid pose control dimensions or space");
        for(const auto value:values)weight(value);
        Object3DRigOutputs output;output.joints.resize(program_->joints.size());output.blendShapes.resize(program_->units.size(),0);
        if(std::all_of(values.begin(),values.end(),[](double value){return value==0;}))return output;
        std::vector<Quaternion> deltas(program_->joints.size()),world(skeleton_->bones().size());
        for(std::size_t u=0;u<values.size();++u)if(values[u]!=0)
            for(std::size_t j=0;j<deltas.size();++j)deltas[j]=product(deltas[j],slerp({},program_->units[u].worldRotationDeltas[j],values[u]));
        for(const auto bone:skeleton_->evaluationOrder())
        {
            const auto &bind=skeleton_->bones()[bone];const auto parent=bind.parent;const auto parentWorld=parent==NoBone?Quaternion{}:world[parent];const auto joint=boneToJoint_[bone];
            if(joint==missing)world[bone]=product(parentWorld,bind.bindTransform.rotation);
            else
            {
                world[bone]=product(deltas[joint],bindWorld_[bone]);const auto local=product(conjugate(parentWorld),world[bone]);
                output.joints[joint].rotation=product(conjugate(bind.bindTransform.rotation),local);
            }
        }
        return output;
    }
  private:
    std::shared_ptr<const Object3DPoseProgram> program_;
    Object3DRigDefinition definition_;
    std::shared_ptr<const Skeleton> skeleton_;
    std::vector<BoneIndex> joints_;
    std::vector<std::size_t> boneToJoint_;
    std::vector<Quaternion> bindWorld_;
};
std::uint32_t crc(std::span<const std::byte> bytes)
{std::uint32_t value=0xffffffffu;for(auto b:bytes){value^=std::to_integer<unsigned>(b);for(unsigned i=0;i<8;++i)value=(value>>1)^((value&1)?0xedb88320u:0u);}return ~value;}
void append(std::vector<std::byte> &out,std::uint64_t value,unsigned count)
{for(unsigned i=0;i<count;++i)out.push_back(static_cast<std::byte>((value>>(8*i))&255));}
class Reader final
{
  public:
    explicit Reader(std::span<const std::byte> bytes):bytes_(bytes){}
    std::uint64_t integer(unsigned count)
    {if(count>bytes_.size()-offset_)throw std::invalid_argument("Truncated pose program");std::uint64_t value=0;for(unsigned i=0;i<count;++i)value|=std::uint64_t(std::to_integer<unsigned>(bytes_[offset_++]))<<(8*i);return value;}
    std::string string()
    {const auto count=integer(4);if(count>1024||count>bytes_.size()-offset_)throw std::invalid_argument("Pose name budget");std::string value(reinterpret_cast<const char*>(bytes_.data()+offset_),count);offset_+=count;return value;}
    void finish(){if(offset_!=bytes_.size())throw std::invalid_argument("Trailing pose program data");}
  private:
    std::span<const std::byte> bytes_;std::size_t offset_=0;
};
constexpr std::array<char,8> magic{'i','i','A','O','P','S','E',0};
}
void validateObject3DPoseProgram(const Object3DPoseProgram &program)
{
    if(program.joints.empty()||program.joints.size()>4096||program.units.empty()||program.units.size()>1024||program.units.size()>1'048'576/program.joints.size())throw std::length_error("Pose joint/unit budget");
    names(program.joints);std::vector<std::string> controls;
    for(const auto &unit:program.units)
    {
        controls.push_back(unit.name);if(unit.name.size()>1019||unit.worldRotationDeltas.size()!=program.joints.size())throw std::invalid_argument("Pose unit dimensions or morph name budget");
        for(const auto q:unit.worldRotationDeltas)
        {const auto normal=q.normalized();if(std::max({std::abs(q.x-normal.x),std::abs(q.y-normal.y),std::abs(q.z-normal.z),std::abs(q.w-normal.w)})>1e-6)throw std::invalid_argument("Pose rotation must be normalized");}
    }
    names(controls);
}
Object3DPoseProgram makeObject3DPoseProgram(const BvhMotion &motion,std::span<const PoseUnitFrame> frames,std::size_t neutralFrame,double angularToleranceRadians)
{
    using namespace character_detail;const auto rest=motion.frame(neutralFrame);const auto neutral=motion.worldRotations(neutralFrame);Object3DPoseProgram result;
    if(!std::isfinite(angularToleranceRadians)||angularToleranceRadians<0||angularToleranceRadians>std::numbers::pi)throw std::invalid_argument("Invalid pose angular tolerance");
    for(const auto &joint:motion.joints())result.joints.push_back(joint.name);
    if(frames.empty()||frames.size()>1024)throw std::length_error("Pose frame selection budget");
    for(const auto &selection:frames)
    {
        auto positions=motion.frame(selection.frame);std::vector<Quaternion> current(positions.size());PoseRigUnit unit;unit.name=selection.name;
        for(std::size_t j=0;j<positions.size();++j)
        {
            const auto d=sub(positions[j].position,rest[j].position);if(length(d)>1e-9)throw std::invalid_argument("Rotation pose units cannot contain translated channels");
            const auto difference=product(positions[j].rotation,conjugate(rest[j].rotation));
            if(2*std::atan2(std::hypot(difference.x,difference.y,difference.z),std::abs(difference.w))<=angularToleranceRadians)positions[j].rotation=rest[j].rotation;
            const auto parent=motion.joints()[j].parent;current[j]=parent==NoBone?positions[j].rotation:product(current[parent],positions[j].rotation);
            unit.worldRotationDeltas.push_back(product(current[j],conjugate(neutral[j])));
        }
        result.units.push_back(std::move(unit));
    }
    validateObject3DPoseProgram(result);return result;
}
std::string object3DPoseMorphName(std::string_view unit){return "pose/"+std::string(unit);}
std::vector<std::byte> exportObject3DPoseProgram(const Object3DPoseProgram &program,std::size_t maximumBytes)
{
    validateObject3DPoseProgram(program);if(maximumBytes<24)throw std::length_error("Pose byte budget");std::vector<std::byte> payload;
    const auto string=[&](const std::string &value){append(payload,value.size(),4);for(auto c:value)payload.push_back(static_cast<std::byte>(static_cast<unsigned char>(c)));};
    append(payload,program.joints.size(),4);append(payload,program.units.size(),4);for(const auto &name:program.joints)string(name);
    for(const auto &unit:program.units){string(unit.name);for(const auto q:unit.worldRotationDeltas)for(const auto value:{q.x,q.y,q.z,q.w})append(payload,std::bit_cast<std::uint64_t>(value),8);}
    if(payload.size()>maximumBytes-24)throw std::length_error("Pose byte budget");std::vector<std::byte> result;for(auto c:magic)result.push_back(static_cast<std::byte>(c));append(result,1,4);append(result,payload.size(),8);append(result,crc(payload),4);result.insert(result.end(),payload.begin(),payload.end());return result;
}
Object3DPoseProgram importObject3DPoseProgram(std::span<const std::byte> bytes,std::size_t maximumBytes)
{
    if(bytes.size()<24||bytes.size()>maximumBytes||std::memcmp(bytes.data(),magic.data(),8)!=0)throw std::invalid_argument("Invalid pose program header");Reader header(bytes.subspan(8,16));
    if(header.integer(4)!=1||header.integer(8)!=bytes.size()-24||header.integer(4)!=crc(bytes.subspan(24)))throw std::invalid_argument("Invalid pose version, size or checksum");
    Reader reader(bytes.subspan(24));const auto joints=reader.integer(4),units=reader.integer(4);
    if(!joints||joints>4096||!units||units>1024||units>1'048'576/joints)throw std::length_error("Pose joint/unit budget");
    Object3DPoseProgram result;for(std::size_t j=0;j<joints;++j)result.joints.push_back(reader.string());
    for(std::size_t u=0;u<units;++u){PoseRigUnit unit;unit.name=reader.string();for(std::size_t j=0;j<joints;++j)unit.worldRotationDeltas.push_back({std::bit_cast<double>(reader.integer(8)),std::bit_cast<double>(reader.integer(8)),std::bit_cast<double>(reader.integer(8)),std::bit_cast<double>(reader.integer(8))});result.units.push_back(std::move(unit));}
    reader.finish();validateObject3DPoseProgram(result);return result;
}
Object3DRigHandle createObject3DPoseRig(const Object3DHandle &object,const Object3DPoseProgram &program,std::span<const BoneIndex> mapping)
{return object.engine().createRigSession(object,object3d_detail::bindPoseProgramDriver(object.get().data(),program,mapping),mapping);}
namespace object3d_detail
{
bool isPoseProgramDriver(const Object3DRigDriver &driver)noexcept{return dynamic_cast<const PoseDriver*>(&driver)!=nullptr;}
std::unique_ptr<Object3DRigDriver> makePoseProgramDriver(Object3DPoseProgram program)
{validateObject3DPoseProgram(program);return std::make_unique<PoseDriver>(std::make_shared<const Object3DPoseProgram>(std::move(program)));}
std::unique_ptr<Object3DRigDriver> bindPoseProgramDriver(const Object3DData &data,const Object3DPoseProgram &program,std::span<const BoneIndex> mapping)
{validateObject3DPoseProgram(program);return std::make_unique<PoseDriver>(std::make_shared<const Object3DPoseProgram>(program),data,mapping);}
}
} // namespace iiActorObject
