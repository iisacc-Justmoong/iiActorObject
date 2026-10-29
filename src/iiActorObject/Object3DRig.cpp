#include "Object3DRig.h"
#include "CharacterMath.h"
#include <unordered_set>

namespace iiActorObject
{
using namespace character_detail;
namespace
{
void names(std::span<const std::string> values)
{
    if (values.size() > 65535) throw std::length_error("Rig channel count exceeds 65535");
    std::unordered_set<std::string_view> seen;
    for (const auto &value : values)
        if (value.empty() || value.size() > 1024 || value.find('\0') != std::string::npos ||
            !seen.insert(value).second)
            throw std::invalid_argument("Invalid or duplicate rig channel name");
}
void finiteValues(std::span<const double> values)
{
    for (const auto value : values)
        if (!std::isfinite(value)) throw std::invalid_argument("Nonfinite rig value");
}
}
Object3DRigDriver::~Object3DRigDriver() = default;
std::unique_ptr<Object3DRigDriver> Object3DRigDriver::cloneFor(const RigSourceSnapshot &) const{return clone();}
std::vector<std::byte> Object3DRigDriver::sourceBytes() const { return {}; }
RigSourceExportResult Object3DRigDriver::serializeSource(
    std::span<const RigSourceSnapshot>,RigSourceExportOptions) const
{
    throw std::logic_error("This rig backend cannot serialize source snapshots");
}
Object3DRigSession::Object3DRigSession(Object3DHandle object,
                                     std::unique_ptr<Object3DRigDriver> driver,
                                     std::span<const BoneIndex> jointMapping)
    : object_(std::move(object)), driver_(std::move(driver))
{
    if (!object_.valid() || !driver_) throw std::invalid_argument("A rig needs a live core object and driver");
    definition_ = driver_->definition();
    if (definition_.lodCount == 0 || definition_.lodCount > 65535)
        throw std::invalid_argument("Invalid rig LOD count");
    names(definition_.joints);
    names(definition_.rawControls);
    names(definition_.guiControls);
    names(definition_.blendShapes);
    names(definition_.animatedMaps);
    if ((!definition_.rawControlDefaults.empty() &&
         definition_.rawControlDefaults.size() != definition_.rawControls.size()) ||
        (!definition_.guiControlDefaults.empty() &&
         definition_.guiControlDefaults.size() != definition_.guiControls.size()))
        throw std::invalid_argument("Rig default control dimensions do not match its channels");
    finiteValues(definition_.rawControlDefaults);
    finiteValues(definition_.guiControlDefaults);
    if(definition_.drivenJoints)
    {
        std::unordered_set<std::size_t> driven;
        for(const auto index:*definition_.drivenJoints)if(index>=definition_.joints.size()||!driven.insert(index).second)
            throw std::invalid_argument("Invalid driven rig joint index");
    }
    if (!jointMapping.empty() && jointMapping.size() != definition_.joints.size())
        throw std::invalid_argument("Rig joint mapping size does not match its driver");
    std::unordered_set<BoneIndex> mapped;
    for (std::size_t i = 0; i < definition_.joints.size(); ++i)
    {
        const auto bone = jointMapping.empty() ?
            object_.get().skeleton()->findBone(definition_.joints[i]) : jointMapping[i];
        if (bone >= object_.get().data().bones.size() || !mapped.insert(bone).second)
            throw std::invalid_argument("Invalid, missing or duplicate rig joint binding");
        joints_.push_back(bone);
    }
}
const Object3DHandle &Object3DRigSession::object() const noexcept { return object_; }
const Object3DRigDefinition &Object3DRigSession::definition() const noexcept { return definition_; }
std::span<const BoneIndex> Object3DRigSession::jointMapping()const noexcept{return joints_;}
Object3DRigEvaluation Object3DRigSession::evaluate(std::span<const double> controls,
                                                  RigControlSpace space, std::size_t lod)
{
    const auto &asset = object_.get();
    if ((space != RigControlSpace::Raw && space != RigControlSpace::Gui) ||
        lod >= definition_.lodCount ||
        controls.size() != (space == RigControlSpace::Raw ?
            definition_.rawControls.size() : definition_.guiControls.size()))
        throw std::invalid_argument("Invalid rig control space, buffer or LOD");
    finiteValues(controls);
    const auto output = driver_->evaluate(controls, space, lod);
    if (output.joints.size() != joints_.size() ||
        output.blendShapes.size() != definition_.blendShapes.size() ||
        output.animatedMaps.size() != definition_.animatedMaps.size())
        throw std::runtime_error("Rig backend output dimensions disagree with its definition");
    finiteValues(output.blendShapes);
    finiteValues(output.animatedMaps);
    Object3DRigEvaluation result{object_, lod, {}, output.blendShapes, output.animatedMaps};
    for (const auto &bone : asset.data().bones) result.joints.push_back(bone.bindTransform);
    for (std::size_t i = 0; i < joints_.size(); ++i)
    {
        const auto &delta = output.joints[i];
        auto &local = result.joints[joints_[i]];
        local.position = add(local.position, delta.translation);
        local.rotation = product(local.rotation, delta.rotation);
        local.scale = add(local.scale, delta.scale);
        local = skeletalTransform(local);
    }
    return result;
}
void Object3DRigSession::applyTo(iiCharacter &actor, const Object3DRigEvaluation &evaluation) const
{
    const auto &asset = object_.get();
    if (evaluation.object != object_ || actor.pose().skeleton() != asset.skeleton() ||
        evaluation.joints.size() != asset.data().bones.size() ||
        evaluation.blendShapes.size() != definition_.blendShapes.size() ||
        evaluation.animatedMaps.size() != definition_.animatedMaps.size() ||
        evaluation.lod >= definition_.lodCount)
        throw std::invalid_argument("Rig result and actor must belong to the same core object");
    for (const auto &local : evaluation.joints) (void)skeletalTransform(local);
    finiteValues(evaluation.blendShapes);
    finiteValues(evaluation.animatedMaps);
    const auto components = actor.components();
    for (auto *component : components)
        if (auto *mesh = dynamic_cast<SkeletalMeshComponent *>(component);
            mesh && mesh->mesh()->skeleton() != asset.skeleton())
            throw std::invalid_argument("Actor contains a mesh from a different skeleton");
    if(definition_.drivenJoints)
        for(const auto joint:*definition_.drivenJoints)actor.pose().setLocal(joints_[joint],evaluation.joints[joints_[joint]]);
    else for (std::size_t i = 0; i < evaluation.joints.size(); ++i)actor.pose().setLocal(i, evaluation.joints[i]);
    for (auto *component : components)
        if (auto *mesh = dynamic_cast<SkeletalMeshComponent *>(component))
        {
            const auto &morphs = mesh->mesh()->lods()[mesh->lod()].morphTargets;
            for (std::size_t i = 0; i < morphs.size(); ++i)
            {
                const auto found = std::find(definition_.blendShapes.begin(),
                    definition_.blendShapes.end(), morphs[i].name);
                if (found != definition_.blendShapes.end())
                    mesh->setMorphWeight(i, evaluation.blendShapes[
                        static_cast<std::size_t>(found - definition_.blendShapes.begin())]);
            }
        }
}
Object3DRigEvaluation Object3DRigSession::evaluateNamed(std::span<const NamedRigControl> controls,
    RigControlSpace space, std::size_t lod)
{
    return evaluate(resolveNamed(controls,space),space,lod);
}
std::vector<double> Object3DRigSession::resolveNamed(std::span<const NamedRigControl> controls,
    RigControlSpace space) const
{
    if(space!=RigControlSpace::Raw&&space!=RigControlSpace::Gui)throw std::invalid_argument("Invalid rig control space");
    const auto &names = space == RigControlSpace::Raw ? definition_.rawControls : definition_.guiControls;
    auto values = space == RigControlSpace::Raw ? definition_.rawControlDefaults : definition_.guiControlDefaults;
    if (values.empty()) values.resize(names.size(),0);
    std::unordered_set<std::size_t> seen;
    for (const auto &control : controls)
    {
        const auto found = std::find(names.begin(),names.end(),control.name);
        if (found == names.end()) throw std::invalid_argument("Unknown rig control name");
        const auto index = static_cast<std::size_t>(found-names.begin());
        if (!seen.insert(index).second) throw std::invalid_argument("Duplicate named rig control");
        values[index] = control.value;
    }
    finiteValues(values);return values;
}
Object3DRigHandle Object3DRigSession::cloneFor(const Object3DHandle &object,
    std::span<const BoneIndex> jointMapping) const
{
    if (!object_.engine().owns(object))
        throw std::invalid_argument("Rebinding requires an object from the same engine");
    (void)object_.get();
    return object.engine().createRigSession(object, driver_->cloneFor({&object.get(),jointMapping}), jointMapping);
}
std::vector<std::byte> Object3DRigSession::sourceBytes() const
{
    (void)object_.get();
    return driver_->sourceBytes();
}
RigSourceExportResult Object3DRigSession::serializeSource(
    std::span<const Object3DRigHandle> additional,RigSourceExportOptions options) const
{
    if (options.maximumBytes==0||options.maximumMemory==0)
        throw std::invalid_argument("Invalid source export limits");
    std::vector<RigSourceSnapshot> snapshots{{&object_.get(),joints_}};
    std::unordered_set<Object3DId> seen{object_.id()};
    for (const auto &handle:additional)
    {
        const auto &other=handle.get();
        if (!object_.engine().owns(other.object_)||!seen.insert(other.object_.id()).second||
            other.definition_.sourceFormat!=definition_.sourceFormat||other.definition_.joints!=definition_.joints)
            throw std::invalid_argument("Additional source sessions must be distinct compatible objects in the same engine");
        snapshots.push_back({&other.object_.get(),other.joints_});
    }
    if(definition_.sourceFormat=="dna")for(const auto &snapshot:snapshots)
    {
        if(snapshot.asset->data().referencePose)throw std::invalid_argument("Clear the authoring reference pose before rig source export");
        for(const auto &mesh:snapshot.asset->data().meshes)
            if(!mesh.morphDefaults.empty())throw std::invalid_argument("Clear default morph weights before DNA source export");
    }
    return driver_->serializeSource(snapshots,options);
}
} // namespace iiActorObject
