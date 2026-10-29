#include "Object3DAdjustment.h"
#include "Object3DTemplateSurfaceTopology.h"
#include "Object3DTemplateSurfaceMeshes.h"
#include "Object3DTemplateSurfaceResources.h"
#include "Object3DTemplateSurfaceMaterialNative.h"
#include "Object3DTemplateSurfaceReplacementResources.h"
#include "Object3DTemplateSurfaceReplacementLayers.h"
#include "Object3DTemplateSurfaceSlots.h"
#include "Object3DTemplateSurfaceEditRemap.h"
#include "Object3DTemplateSurfaceSelectionRemap.h"
#include "Object3DTemplateSurfaceReferences.h"
#include "Object3DTemplateSurfaceFollowerNative.h"
#include "Object3DSelectionState.h"
#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace iiActorObject
{
namespace
{
std::size_t bindingIndex(const Object3DTemplateDefinition &definition,std::size_t target)
{return object3d_detail::templateSurfaceBindingIndex(definition.surfaces,target);}
std::optional<SurfaceBindingBasis> bindingBasis(const Object3DTemplateDefinition &definition,std::size_t index)
{return object3d_detail::templateSurfaceBindingBasis(definition.surfaceOffsetBases,index);}
object3d_detail::TemplateSurfaceReferences takeSurfaceReferences(Object3DTemplateDefinition &definition)
{
    return {std::move(definition.targets),std::move(definition.joints),std::move(definition.grounding),
        std::move(definition.surfaces),std::move(definition.surfaceOffsetBases),std::move(definition.fittings)};
}
void putSurfaceReferences(Object3DTemplateDefinition &definition,object3d_detail::TemplateSurfaceReferences source)
{
    definition.targets=std::move(source.targets);definition.joints=std::move(source.joints);definition.grounding=std::move(source.grounding);
    definition.surfaces=std::move(source.surfaces);definition.surfaceOffsetBases=std::move(source.bases);definition.fittings=std::move(source.fittings);
}
}

bool Object3DEditor::addTemplateSurfaces(const Object3DTemplateHandle &alternative,
    std::span<const std::size_t> meshes,std::span<const std::size_t> visibilityMeshes)
{
    return changeTemplateSurfaces(alternative,{},meshes,visibilityMeshes);
}
bool Object3DEditor::changeTemplateSurfaces(const Object3DTemplateHandle &alternative,
    std::span<const std::size_t> removedMeshes,std::span<const std::size_t> addedMeshes,
    std::span<const std::size_t> visibilityMeshes,const TemplateSurfaceRemovalOptions &options)
{
    if(addedMeshes.empty())return removeTemplateSurfaces(alternative,removedMeshes,visibilityMeshes,options);
    if(!layered())throw std::logic_error("Template surface addition requires a template editor");
    const auto &next=alternative.get().definition();
    if(!owner_.owns(next.base))throw std::invalid_argument("Template surface addition requires the same engine");
    auto state=copyLayers();auto definition=state->program.get().definition();
    const auto &before=definition.base.get().data(),&after=next.base.get().data();
    auto slots=object3d_detail::planTemplateSurfaceSlots(object3d_detail::TemplateSurfaceOperation::Change,
        before.meshes.size(),after.meshes.size(),before.bones,after.bones,removedMeshes,addedMeshes,visibilityMeshes);
    const auto &added=slots.addedSlots,&removed=slots.sourceSlots,&visibility=slots.visibility;
    auto oldToNew=std::move(slots.oldToNew),newToOld=std::move(slots.newToOld);
    for(std::size_t target=0;target<after.meshes.size();++target)if(newToOld[target])
    {
        const auto old=*newToOld[target];const auto &a=before.meshes[old],&b=after.meshes[target];
        if(visibility.contains(old))
        {
            if(a.name!=b.name||!a.topology||!b.topology)throw std::invalid_argument("Template visibility requires a retained source mesh");
            object3d_detail::validateTemplateVisibilityLayout(*a.topology,*b.topology,true);
            if(definition.grounding&&definition.grounding->mesh==old)object3d_detail::preserveTemplateGroundingSupport(definition.grounding->supportPoints,*a.topology,*b.topology);
        }
        else object3d_detail::validateRetainedTemplateLayout(a,b);
    }
    for(const auto source:removed)(void)bindingIndex(definition,source);
    for(const auto target:added)(void)bindingIndex(next,target);
    object3d_detail::validateAddedTemplateSurfaceReferences(next.targets,next.joints,next.grounding?&*next.grounding:nullptr,added);
    putSurfaceReferences(definition,object3d_detail::remapTemplateSurfaceReferences(takeSurfaceReferences(definition),
        oldToNew,removed,visibility,before.meshes,after.meshes,next.fittings,added));
    const object3d_detail::TemplateSurfaceEditRemapper editRemap(oldToNew,removed,options.discardRemovedEdits,
        object3d_detail::TemplateSurfaceRemapMode::Change);
    std::vector<std::shared_ptr<object3d_detail::Adjustment>> adjustments(state->layers.size());
    for(std::size_t i=0;i<state->layers.size();++i)
    {
        auto &layer=state->layers[i];auto update=editRemap.adjustment(layer.adjustment);
        if(update.writable)
        {
            adjustments[i]=std::move(update.writable);layer.adjustment=std::move(update.value);
        }
        if(layer.pointMasks)
        {
            auto masks=editRemap.masks(layer.pointMasks,object3d_detail::TemplateSurfaceMaskKind::Layer);
            if(masks.changed)layer.pointMasks=std::move(masks.value);
        }
    }
    const auto oldSelections=history_[cursor_].selections;
    auto selectionUpdate=object3d_detail::remapTemplateSurfaceSelections(oldSelections,editRemap);
    auto selections=std::move(selectionUpdate.value);const bool selectionChanged=selectionUpdate.changed;
    auto base=before;base.meshes.clear();
    base.meshes=object3d_detail::assembleTemplateSurfaceMeshes(before.meshes,after.meshes,newToOld,added,visibility);
    std::vector<object3d_detail::TemplateSurfaceResourceLayer> resourceLayers;
    resourceLayers.reserve(adjustments.size());
    for(const auto &adjustment:adjustments)if(adjustment)
        resourceLayers.push_back({adjustment->images?&*adjustment->images:nullptr,
                                  adjustment->materials?&*adjustment->materials:nullptr});
    object3d_detail::TemplateSurfaceResources resources(base,after,resourceLayers);
    object3d_detail::TemplateSurfaceMaterialNative materialMapper(resources);
    object3d_detail::linkRetainedTemplateSurfaceMaterials(before.meshes,after.meshes,newToOld,materialMapper);
    object3d_detail::remapAddedTemplateSurfaceMaterials(base.meshes,added,materialMapper);
    const object3d_detail::TemplateSurfaceFollowerNativeOperations followerOperations;
    const auto followers=object3d_detail::addTemplateSurfaceFollowers(base,before,after,object().get().data(),
        newToOld,added,next.surfaces,next.surfaceOffsetBases,followerOperations);
    for(const auto &follower:followers)
    {
        if(follower.basis&&definition.surfaceOffsetBases.empty())definition.surfaceOffsetBases.resize(definition.surfaces.size());
        definition.surfaces.push_back(follower.binding);
        if(!definition.surfaceOffsetBases.empty())definition.surfaceOffsetBases.push_back(follower.basis);
    }
    Object3DHandle newBase;Object3DTemplateHandle program;
    try
    {
        newBase=owner_.createObject(std::move(base));definition.base=newBase;program=owner_.createTemplate(std::move(definition));state->program=program;
        auto result=rebuild(*state);
        const auto label=removed.empty()?"Add template surfaces":"Change template surfaces";
        if(!selectionChanged)publish(std::move(result),label,std::move(state));
        else
        {
            checkRevision();object3d_detail::validateRemappedPointSelectionTransition(object().get().data(),result,*oldSelections,*selections,oldToNew,
                removed.empty()?object3d_detail::SelectionRemapMode::Insertion:object3d_detail::SelectionRemapMode::Mixed);
            std::vector<Entry> history(history_.begin(),history_.begin()+static_cast<std::ptrdiff_t>(cursor_+1));
            history.reserve(history.size()+1);auto updated=owner_.createObject(std::move(result));
            auto mapping=std::make_shared<const std::vector<std::optional<std::size_t>>>(std::move(oldToNew));
            history.push_back({std::move(updated),label,std::move(state),std::move(selections),std::move(mapping)});
            if(history.size()>limit_+1)history.erase(history.begin());
            history_.swap(history);cursor_=history_.size()-1;++revision_;history.clear();owner_.collectGarbage();
        }
    }
    catch(...){if(program.valid())owner_.eraseTemplate(program.id());if(newBase.valid())owner_.eraseObject(newBase.id());throw;}
    return true;
}

bool Object3DEditor::removeTemplateSurfaces(const Object3DTemplateHandle &alternative,
    std::span<const std::size_t> meshes,std::span<const std::size_t> visibilityMeshes,
    const TemplateSurfaceRemovalOptions &options)
{
    if(meshes.empty())return replaceTemplateSurfaces(alternative,{},visibilityMeshes);
    if(!layered())throw std::logic_error("Template surface removal requires a template editor");
    const auto &next=alternative.get().definition();
    if(!owner_.owns(next.base))throw std::invalid_argument("Template surface removal requires the same engine");
    auto state=copyLayers();auto definition=state->program.get().definition();
    const auto &before=definition.base.get().data(),&after=next.base.get().data();
    auto slots=object3d_detail::planTemplateSurfaceSlots(object3d_detail::TemplateSurfaceOperation::Remove,
        before.meshes.size(),after.meshes.size(),before.bones,after.bones,meshes,{},visibilityMeshes);
    const auto &removed=slots.sourceSlots,&visibility=slots.visibility;
    auto map=std::move(slots.oldToNew);
    for(std::size_t old=0;old<before.meshes.size();++old)if(map[old])
    {
        const auto &a=before.meshes[old],&b=after.meshes[*map[old]];
        if(visibility.contains(old))
        {
            if(a.name!=b.name||!a.topology||!b.topology)throw std::invalid_argument("Template visibility requires a retained source mesh");
            object3d_detail::validateTemplateVisibilityLayout(*a.topology,*b.topology,true);
            if(definition.grounding&&definition.grounding->mesh==old)object3d_detail::preserveTemplateGroundingSupport(definition.grounding->supportPoints,*a.topology,*b.topology);
        }
        else object3d_detail::validateRetainedTemplateLayout(a,b);
    }
    for(const auto mesh:removed)(void)bindingIndex(definition,mesh);
    putSurfaceReferences(definition,object3d_detail::remapTemplateSurfaceReferences(takeSurfaceReferences(definition),
        map,removed,visibility,before.meshes,after.meshes,{},{}));
    const object3d_detail::TemplateSurfaceEditRemapper editRemap(map,removed,options.discardRemovedEdits,
        object3d_detail::TemplateSurfaceRemapMode::Removal);
    for(auto &layer:state->layers)
    {
        auto adjustment=editRemap.adjustment(layer.adjustment);
        if(adjustment.writable)layer.adjustment=std::move(adjustment.value);
        if(layer.pointMasks)
        {
            auto masks=editRemap.masks(layer.pointMasks,object3d_detail::TemplateSurfaceMaskKind::Layer);
            if(masks.changed)layer.pointMasks=std::move(masks.value);
        }
    }
    const auto oldSelections=history_[cursor_].selections;
    auto selectionUpdate=object3d_detail::remapTemplateSurfaceSelections(oldSelections,editRemap);
    auto selections=std::move(selectionUpdate.value);const bool selectionChanged=selectionUpdate.changed;
    auto base=before;base.meshes.clear();
    base.meshes=object3d_detail::assembleTemplateSurfaceMeshes(before.meshes,after.meshes,slots.newToOld,{},visibility);
    Object3DHandle newBase;Object3DTemplateHandle program;
    try
    {
        newBase=owner_.createObject(std::move(base));definition.base=newBase;program=owner_.createTemplate(std::move(definition));state->program=program;
        auto result=rebuild(*state);
        if(!selectionChanged)publish(std::move(result),"Remove template surfaces",std::move(state));
        else
        {
            checkRevision();object3d_detail::validateRemappedPointSelectionTransition(object().get().data(),result,*oldSelections,*selections,map);
            std::vector<Entry> history(history_.begin(),history_.begin()+static_cast<std::ptrdiff_t>(cursor_+1));
            history.reserve(history.size()+1);auto updated=owner_.createObject(std::move(result));
            auto stored=std::make_shared<const std::vector<std::optional<std::size_t>>>(std::move(map));
            history.push_back({std::move(updated),"Remove template surfaces",std::move(state),std::move(selections),std::move(stored)});
            if(history.size()>limit_+1)history.erase(history.begin());
            history_.swap(history);cursor_=history_.size()-1;++revision_;history.clear();owner_.collectGarbage();
        }
    }
    catch(...){if(program.valid())owner_.eraseTemplate(program.id());if(newBase.valid())owner_.eraseObject(newBase.id());throw;}
    return true;
}

bool Object3DEditor::replaceTemplateSurfaces(const Object3DTemplateHandle &alternative,
    std::span<const std::size_t> meshes,std::span<const std::size_t> visibilityMeshes)
{
    if(!layered())throw std::logic_error("Template surface replacement requires a template editor");
    const auto &next=alternative.get().definition();
    if(!owner_.owns(next.base))throw std::invalid_argument("Template surface replacement requires the same engine");
    auto state=copyLayers();auto definition=state->program.get().definition();
    const auto &before=definition.base.get().data(),&after=next.base.get().data();
    const auto slots=object3d_detail::planTemplateSurfaceSlots(object3d_detail::TemplateSurfaceOperation::Replace,
        before.meshes.size(),after.meshes.size(),before.bones,after.bones,meshes,{},visibilityMeshes);
    const auto &replaced=slots.sourceSlots,&visibility=slots.visibility;
    bool visibilityChanged=false;
    for(std::size_t i=0;i<before.meshes.size();++i)
    {
        if(replaced.contains(i))continue;
        const auto &old=before.meshes[i],&candidate=after.meshes[i];
        if(old.name!=candidate.name||old.topology.has_value()!=candidate.topology.has_value())
            throw std::invalid_argument("Retained template mesh identity changed");
        if(old.topology)
        {
            if(visibility.contains(i))
            {
                object3d_detail::validateTemplateVisibilityLayout(*old.topology,*candidate.topology,true);
                if(definition.grounding&&definition.grounding->mesh==i)
                    object3d_detail::preserveTemplateGroundingSupport(definition.grounding->supportPoints,*old.topology,*candidate.topology);
                visibilityChanged|=old.topology->layout->definition().hiddenPolygons!=candidate.topology->layout->definition().hiddenPolygons;
            }
            else object3d_detail::validateRetainedTemplateLayout(old,candidate);
        }
        else if(visibility.contains(i)||old.geometry.triangles!=candidate.geometry.triangles||old.geometry.vertices.size()!=candidate.geometry.vertices.size())
            throw std::invalid_argument("Retained template render topology changed");
    }
    if(replaced.empty()&&!visibilityChanged)return false;
    object3d_detail::validateReplacementTemplateSurfaceReferences(definition.targets,definition.joints,
        definition.grounding?&*definition.grounding:nullptr,definition.surfaces,replaced);
    definition.surfaces=object3d_detail::remapReplacementTemplateVisibility(std::move(definition.surfaces),
        replaced,visibility,before.meshes,after.meshes);
    object3d_detail::validateReplacementTemplateSurfaceFittings(definition.fittings,replaced);
    if(const auto &selection=history_[cursor_].selections)
        for(const auto &entry:selection->entries)if(entry.masks)for(const auto &mask:*entry.masks)
            if(replaced.contains(mask.mesh))throw std::invalid_argument("Replacement surface has a named point selection");
    for(const auto &layer:state->layers)
    {
        if(std::any_of(layer.adjustment->meshes.begin(),layer.adjustment->meshes.end(),[&](const auto &mesh){return replaced.contains(mesh.mesh);}))
            throw std::invalid_argument("Replacement surface has edit layers");
        if(layer.pointMasks&&std::any_of(layer.pointMasks->begin(),layer.pointMasks->end(),[&](const auto &mask){return replaced.contains(mask.mesh);}))
            throw std::invalid_argument("Replacement surface has layer masks");
    }
    struct BindingSwap final {std::size_t index;SurfaceBinding binding;std::optional<SurfaceBindingBasis> basis;};
    std::vector<BindingSwap> bindings;
    object3d_detail::TemplateSurfaceReplacementResources resourcePlan(before,after);
    for(const auto mesh:replaced)
    {
        resourcePlan.add(mesh);
        const auto previousBinding=bindingIndex(definition,mesh),nextBinding=bindingIndex(next,mesh);
        const auto &incoming=next.surfaces[nextBinding];
        if(incoming.sourceMesh!=definition.surfaces[previousBinding].sourceMesh||replaced.contains(incoming.sourceMesh))
            throw std::invalid_argument("Replacement follower source changed");
        bindings.push_back({previousBinding,incoming,bindingBasis(next,nextBinding)});
    }
    const auto &materials=resourcePlan.materials();
    const auto &images=resourcePlan.images();
    for(auto &layer:state->layers)
        layer.adjustment=object3d_detail::prepareTemplateSurfaceReplacementLayer(layer.adjustment,resourcePlan,before);
    auto base=before;
    for(const auto mesh:visibility)
        base.meshes[mesh]=object3d_detail::templateSurfaceRetainedMesh(before.meshes[mesh],after.meshes[mesh],true);
    for(const auto &item:materials)base.materials[item.index]=item.replacement;
    for(const auto &item:images)base.images[item.index]=item.replacement;
    for(const auto mesh:replaced)
        base.meshes[mesh]=object3d_detail::templateSurfaceReplacementMesh(after.meshes[mesh],before.meshes[mesh].material);
    const object3d_detail::TemplateSurfaceFollowerNativeOperations followerOperations;
    for(const auto &item:bindings)
    {
        definition.surfaces[item.index]=item.binding;
        if(item.basis&&definition.surfaceOffsetBases.empty())definition.surfaceOffsetBases.resize(definition.surfaces.size());
        if(!definition.surfaceOffsetBases.empty())definition.surfaceOffsetBases[item.index]=item.basis;
        const auto &current=object().get().data().meshes.at(item.binding.sourceMesh);
        object3d_detail::applyTemplateSurfaceFollower(base,item.binding,item.basis,&current,
            object3d_detail::TemplateSurfaceFollowerKind::Replacement,followerOperations);
    }
    Object3DHandle newBase;Object3DTemplateHandle newProgram;
    try
    {
        newBase=owner_.createObject(std::move(base));definition.base=newBase;newProgram=owner_.createTemplate(std::move(definition));state->program=newProgram;
        auto result=rebuild(*state);publish(std::move(result),"Replace template surfaces",std::move(state));
    }
    catch(...){if(newProgram.valid())owner_.eraseTemplate(newProgram.id());if(newBase.valid())owner_.eraseObject(newBase.id());throw;}
    return true;
}
} // namespace iiActorObject
