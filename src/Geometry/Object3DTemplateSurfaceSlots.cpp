#include "Object3DTemplateSurfaceSlots.h"
#include "Skeleton.h"
#include <stdexcept>

namespace iiActorObject::object3d_detail
{
namespace
{
void validateBones(std::span<const Bone> oldBones,std::span<const Bone> newBones,const char *message)
{
    for(std::size_t i=0;i<oldBones.size();++i)
        if(oldBones[i].name!=newBones[i].name||oldBones[i].parent!=newBones[i].parent)
            throw std::invalid_argument(message);
}
}
TemplateSurfaceSlotPlan planTemplateSurfaceSlots(TemplateSurfaceOperation operation,
    std::size_t oldCount,std::size_t newCount,std::span<const Bone> oldBones,std::span<const Bone> newBones,
    std::span<const std::size_t> sourceSlots,std::span<const std::size_t> addedSlots,
    std::span<const std::size_t> visibilitySlots)
{
    if(operation!=TemplateSurfaceOperation::Change&&operation!=TemplateSurfaceOperation::Remove&&operation!=TemplateSurfaceOperation::Replace)
        throw std::invalid_argument("Unknown template surface operation");
    if(operation!=TemplateSurfaceOperation::Change&&!addedSlots.empty())throw std::invalid_argument("Unexpected added template surfaces");
    TemplateSurfaceSlotPlan result;
    if(operation==TemplateSurfaceOperation::Replace)
    {
        if(oldCount!=newCount||oldBones.size()!=newBones.size())
            throw std::invalid_argument("Template surface replacement requires stable mesh slots and skeleton");
        validateBones(oldBones,newBones,"Template surface replacement skeleton identities changed");
        if(sourceSlots.size()>32||visibilitySlots.size()>32)throw std::length_error("Template surface replacement limit");
    }
    else if(sourceSlots.size()>32||addedSlots.size()>32||visibilitySlots.size()>32)
    {
        if(operation==TemplateSurfaceOperation::Change)throw std::length_error("Template surface change limit");
        throw std::length_error("Template surface removal limit");
    }
    for(const auto slot:addedSlots)if(slot>=newCount||!result.addedSlots.insert(slot).second)
        throw std::invalid_argument("Invalid or duplicate added template surface");
    for(const auto slot:sourceSlots)if(slot>=oldCount||!result.sourceSlots.insert(slot).second)
    {
        if(operation==TemplateSurfaceOperation::Replace)throw std::invalid_argument("Invalid or duplicate template surface slot");
        throw std::invalid_argument("Invalid or duplicate removed template surface");
    }
    for(const auto slot:visibilitySlots)if(slot>=oldCount||result.sourceSlots.contains(slot)||!result.visibility.insert(slot).second)
        throw std::invalid_argument("Invalid or duplicate template visibility slot");
    if(operation==TemplateSurfaceOperation::Replace)return result;
    if(newCount<result.addedSlots.size()||oldCount-result.sourceSlots.size()!=newCount-result.addedSlots.size()||oldBones.size()!=newBones.size())
    {
        if(operation==TemplateSurfaceOperation::Change)throw std::invalid_argument("Template change requires retained mesh slots and skeleton");
        throw std::invalid_argument("Template removal requires retained mesh slots and skeleton");
    }
    validateBones(oldBones,newBones,operation==TemplateSurfaceOperation::Change?
        "Template addition skeleton identities changed":"Template removal skeleton identities changed");
    result.oldToNew.resize(oldCount);result.newToOld.resize(newCount);
    std::size_t old=0;
    for(std::size_t target=0;target<newCount;++target)if(!result.addedSlots.contains(target))
    {
        while(old<oldCount&&result.sourceSlots.contains(old))++old;
        if(old>=oldCount)throw std::invalid_argument("Template change omits a retained mesh");
        result.oldToNew[old]=target;result.newToOld[target]=old++;
    }
    while(old<oldCount&&result.sourceSlots.contains(old))++old;
    if(old!=oldCount)throw std::invalid_argument("Template change omits a retained mesh");
    return result;
}
} // namespace iiActorObject::object3d_detail
