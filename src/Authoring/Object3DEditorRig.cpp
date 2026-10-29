#include "Object3DEditorRig.h"
#include "Object3DJointIdentityInternal.h"
#include <set>
#include <stdexcept>

namespace iiActorObject
{
namespace
{
bool sameChannels(const Object3DRigDefinition &a,const Object3DRigDefinition &b)
{
    // A backend may recompute drivenJoints for the new bind frame.
    return a.sourceFormat==b.sourceFormat&&a.joints==b.joints&&a.rawControls==b.rawControls&&
        a.guiControls==b.guiControls&&a.blendShapes==b.blendShapes&&a.animatedMaps==b.animatedMaps&&
        a.rawControlDefaults==b.rawControlDefaults&&a.guiControlDefaults==b.guiControlDefaults&&a.lodCount==b.lodCount;
}
}
Object3DEditorRigSet::Object3DEditorRigSet(Object3DEditorHandle editor,std::span<const Object3DRigHandle> rigs,
    Object3DEditorRigOptions options):editor_(std::move(editor)),rigs_(rigs.begin(),rigs.end()),options_(options)
{
    if(options_.jointIdentity!=EditedRigJointIdentity::RequireEvidence&&options_.jointIdentity!=EditedRigJointIdentity::TrustStableIndices)
        throw std::invalid_argument("Invalid edited rig joint identity policy");
    if(rigs_.empty()||rigs_.size()>256)throw std::length_error("Edited rig set size must be between 1 and 256");
    const auto object=editor_.get().object();(void)object.get();std::set<Object3DId> ids;
    for(const auto &handle:rigs_)
        if(handle.get().object()!=object||!ids.insert(handle.id()).second)
            throw std::invalid_argument("Edited rigs must be distinct sessions for the editor's current object");
}
const Object3DEditorHandle &Object3DEditorRigSet::editor() const noexcept{return editor_;}
Object3DHandle Object3DEditorRigSet::boundObject() const
{
    (void)editor_.get();const auto source=rigs_.front().get().object();(void)source.get();
    for(const auto &handle:rigs_)if(handle.get().object()!=source)
        throw std::logic_error("Edited rig set sessions no longer share a source");
    return source;
}
bool Object3DEditorRigSet::synchronize()
{
    const auto target=editor_.get().object();const auto &destination=target.get();
    const auto source=boundObject();const auto &previous=source.get();
    if(source==target)return false;
    if(!source.engine().owns(target))throw std::invalid_argument("Edited rig target belongs to a different engine");
    object3d_detail::validateEditedJointIdentity(previous,destination,options_.jointIdentity);
    std::vector<Object3DRigHandle> staged;staged.reserve(rigs_.size());const auto owner=target.engine();
    try
    {
        for(const auto &handle:rigs_)
        {
            auto &prior=handle.get();staged.push_back(prior.cloneFor(target,prior.jointMapping()));
            if(!sameChannels(prior.definition(),staged.back().get().definition()))
                throw std::invalid_argument("Edited rig backend changed its control or output channels");
        }
    }
    catch(...)
    {
        for(const auto &handle:staged)if(handle.valid())owner.eraseRig(handle.id());
        throw;
    }
    rigs_.swap(staged);staged.clear();owner.collectGarbage();return true;
}
std::span<const Object3DRigHandle> Object3DEditorRigSet::rigs(){(void)synchronize();return rigs_;}
Object3DRigHandle Object3DEditorRigSet::rig(std::size_t index)
{
    if(index>=rigs_.size())throw std::out_of_range("Edited rig index is out of range");
    (void)synchronize();return rigs_[index];
}
Object3DRigEvaluation Object3DEditorRigSet::evaluate(std::size_t index,std::span<const double> controls,RigControlSpace space,std::size_t lod)
{return rig(index).get().evaluate(controls,space,lod);}
Object3DRigEvaluation Object3DEditorRigSet::evaluateNamed(std::size_t index,std::span<const NamedRigControl> controls,RigControlSpace space,std::size_t lod)
{return rig(index).get().evaluateNamed(controls,space,lod);}
}
