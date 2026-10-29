#include "Object3DProjectState.h"
#include "Object3DProjectRigArchive.h"
#include "Object3DProjectTemplateAdapter.h"
#include "Object3DPoseRigBackend.h"
#include "Object3DTemplateMorphBackend.h"
#include "Object3DProgressiveMorphBackend.h"
#include "Object3DPoseCorrectivesBackend.h"
#include "Object3DDnaBackend.h"
#include <map>
#include <set>

namespace iiActorObject::object3d_detail
{
ProjectArchive ProjectAccess::capture(const Object3DEditorHandle &handle,std::span<const Object3DRigHandle> rigs)
{
    const auto &editor=handle.get();ProjectArchive graph;const auto owner=editor.object().engine();
    std::map<Object3DId,std::size_t> assetIds,programIds;
    std::map<const LayerState*,std::size_t> stateIds;
    std::map<const PointSelectionState*,std::size_t> selectionIds;
    std::map<const Adjustment*,std::size_t> adjustmentIds;
    std::map<const std::vector<MeshPointMask>*,std::size_t> maskIds;
    const auto mask=[&](const std::shared_ptr<const std::vector<MeshPointMask>> &values)->std::optional<std::size_t>
    {
        if(!values)return {};
        const auto [it,added]=maskIds.emplace(values.get(),graph.maskSets.size());
        if(added)graph.maskSets.push_back(*values);return it->second;
    };
    auto asset=[&](const Object3DHandle &object)
    {
        if(!owner.owns(object))throw std::invalid_argument("Project resources require the editor's owning engine");
        const auto [it,added]=assetIds.emplace(object.id(),graph.assets.size());
        if(added)graph.assets.push_back(object.get().data());return it->second;
    };
    auto program=[&](const Object3DTemplateHandle &p)
    {
        if(!owner.owns(p.get().definition().base))throw std::invalid_argument("Project templates require the editor's owning engine");
        const auto [it,added]=programIds.emplace(p.id(),graph.programs.size());
        if(added){const auto &definition=p.get().definition();const auto base=asset(definition.base);graph.programs.push_back({base,detachProjectTemplate(definition)});}return it->second;
    };
    for(const auto &entry:editor.history_)
    {
        ProjectEntry archived{asset(entry.object),entry.label,{}, {},{}};
        if(!graph.history.empty()&&entry.selectionRemap)archived.selectionRemap=*entry.selectionRemap;
        if(entry.layers)
        {
            const auto [it,added]=stateIds.emplace(entry.layers.get(),graph.states.size());
            if(added)
            {
                ProjectState state;state.program=program(entry.layers->program);state.values=entry.layers->values;state.referencePose=entry.layers->referencePose;
                for(const auto &layer:entry.layers->layers)
                {
                    if(!layer.adjustment)throw std::logic_error("Missing project adjustment");
                    const auto [a,inserted]=adjustmentIds.emplace(layer.adjustment.get(),graph.adjustments.size());
                    if(inserted)graph.adjustments.push_back(*layer.adjustment);
                    state.layers.push_back({layer.info,a->second,mask(layer.pointMasks)});
                }
                graph.states.push_back(std::move(state));
            }
            archived.layers=it->second;
        }
        if(entry.selections)
        {
            const auto [it,added]=selectionIds.emplace(entry.selections.get(),graph.selections.size());
            if(added)
            {
                ProjectSelectionState state;
                for(const auto &selection:entry.selections->entries)state.entries.push_back({selection.name,mask(selection.masks)});
                graph.selections.push_back(std::move(state));
            }
            archived.selections=it->second;
        }
        graph.history.push_back(std::move(archived));
    }
    std::set<Object3DId> rigIds;
    ProjectRigArchiveBuilder rigArchive(graph);
    for(const auto &handle:rigs)
    {
        const auto &rig=handle.get();if(!rigIds.insert(handle.id()).second)throw std::invalid_argument("Project requires distinct rig sessions");
        const bool pose=isPoseProgramDriver(*rig.driver_),morph=isTemplateMorphRigDriver(*rig.driver_),
            corrective=isPoseCorrectiveDriver(*rig.driver_),progressive=isProgressiveMorphRigDriver(*rig.driver_);
        if(!pose&&!morph&&!corrective&&!progressive&&!isDnaProgramDriver(*rig.driver_))throw std::invalid_argument("Project does not support this rig backend");
        const auto index=asset(rig.object_);
        if(progressive)
        {
            rigArchive.appendProgressive(index,rig.joints_,progressiveMorphRigProgram(*rig.driver_));
            continue;
        }
        if(morph)
        {
            const auto &info=templateMorphRigInfo(*rig.driver_);std::optional<std::size_t> templateId;
            if(info.templateProgram.valid())templateId=program(info.templateProgram);
            rigArchive.appendMorph(index,rig.joints_,info.program,templateId);continue;
        }
        const auto source=rig.sourceBytes();
        const auto kind=corrective?ProjectRigByteKind::Corrective:pose?ProjectRigByteKind::Pose:ProjectRigByteKind::Dna;
        rigArchive.appendBytes(kind,index,rig.joints_,source);
    }
    graph.cursor=editor.cursor_;graph.limit=editor.limit_;graph.revision=editor.revision_;graph.nextLayer=editor.nextLayer_;return graph;
}
} // namespace iiActorObject::object3d_detail
