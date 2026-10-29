#include "Object3DProjectState.h"
#include "MeshPointMaskValidation.h"
#include "Object3DProjectAdjustmentValidation.h"
#include "Object3DProjectCodec.h"
#include "Object3DProjectHistorySelection.h"
#include "Object3DProjectRigRestore.h"
#include "Object3DProjectTemplateAdapter.h"
#include "Object3DProjectAnimationValidation.h"
#include <algorithm>
#include <set>

namespace iiActorObject::object3d_detail
{
Object3DProject ProjectAccess::instantiate(const Object3DReference &engine,ProjectArchive &graph,const Object3DProjectOptions &options)
{
    if(graph.history.empty()||graph.limit==0||graph.limit>4096||graph.history.size()>graph.limit+1||graph.cursor>=graph.history.size()||!graph.revision||!graph.nextLayer)
        throw std::invalid_argument("Invalid project editor history");
    if(graph.programs.size()>64||graph.rigs.size()>256||graph.dna.size()>256||graph.posePrograms.size()>256||graph.morphPrograms.size()>256||graph.correctivePrograms.size()>256||graph.progressiveMorphPrograms.size()>256)throw std::length_error("Project program or rig limit");
    if(graph.selections.size()>graph.history.size())throw std::invalid_argument("Unreferenced project selection state");
    std::vector<Object3DHandle> assets;assets.reserve(graph.assets.size());
    for(auto &data:graph.assets)assets.push_back(engine.createObject(std::move(data)));
    std::vector<Object3DTemplateHandle> programs;programs.reserve(graph.programs.size());
    for(auto &p:graph.programs)programs.push_back(engine.createTemplate(bindProjectTemplate(std::move(p.definition),assets.at(p.base))));
    std::vector<std::shared_ptr<const Adjustment>> adjustments;
    for(auto &a:graph.adjustments)adjustments.push_back(std::make_shared<const Adjustment>(std::move(a)));
    std::vector<std::shared_ptr<const std::vector<MeshPointMask>>> maskSets;
    for(auto &m:graph.maskSets){if(m.empty())throw std::invalid_argument("Empty project mask set");maskSets.push_back(std::make_shared<const std::vector<MeshPointMask>>(std::move(m)));}
    std::vector<bool> usedMasks(maskSets.size(),false);
    std::vector<std::shared_ptr<const LayerState>> states;states.reserve(graph.states.size());
    for(auto &s:graph.states)
    {
        auto state=std::make_shared<LayerState>();state->program=programs.at(s.program);state->values=std::move(s.values);state->referencePose=std::move(s.referencePose);
        if(s.layers.size()>4096)throw std::length_error("Project edit layer count");
        std::set<Object3DLayerId> ids;
        auto base=state->program.get().evaluate(state->values);Object3DData jointProbe;jointProbe.bones=base.bones;
        if(!std::is_sorted(state->values.begin(),state->values.end(),[](const auto &a,const auto &b){return a.name<b.name;})||
            std::any_of(state->values.begin(),state->values.end(),[](const auto &v){return v.value==0;}))throw std::invalid_argument("Noncanonical project template values");
        for(const auto &layer:s.layers)
        {
            if(!layer.info.id||layer.info.id>=graph.nextLayer||!ids.insert(layer.info.id).second||layer.info.label.empty()||layer.info.weight<0||layer.info.weight>1)
                throw std::invalid_argument("Invalid project edit layer metadata");
            auto adjustment=adjustments.at(layer.adjustment);validateProjectAdjustmentReferences(*adjustment,base);std::shared_ptr<const std::vector<MeshPointMask>> masks;
            if(layer.masks)
            {
                masks=maskSets.at(*layer.masks);usedMasks[*layer.masks]=true;validateMeshPointMasks(base,*masks);
                if(!std::is_sorted(masks->begin(),masks->end(),[](const auto &a,const auto &b){return a.mesh<b.mesh;})||std::any_of(masks->begin(),masks->end(),[](const auto &m){return std::all_of(m.weights.begin(),m.weights.end(),[](double v){return v==1;});}))throw std::invalid_argument("Noncanonical project point masks");
            }
            // Validate inactive corrections too, without allocating permanent engine resources.
            auto probe=base;std::vector<std::uint8_t> moved(probe.meshes.size(),0);
            applySurfaces(probe,*adjustment,1,moved,masks?std::span<const MeshPointMask>(*masks):std::span<const MeshPointMask>{});applyMeshSurfaceBindingsWithBases(probe,base,state->program.get().definition().surfaces,state->program.get().definition().surfaceOffsetBases,moved);
            bindObject3DTemplateJoints(probe,state->program.get().definition().joints);
            if(layer.info.enabled)applyJoints(jointProbe,*adjustment,layer.info.weight);
            probe.bones=jointProbe.bones;refreshSurfaces(probe,moved);
            applyNormalSurfaces(probe,*adjustment,1,masks?std::span<const MeshPointMask>(*masks):std::span<const MeshPointMask>{});
            probe.referencePose.reset();Object3DAsset validated(std::move(probe));
            state->layers.push_back({layer.info,std::move(adjustment),std::move(masks)});
        }
        states.push_back(std::move(state));
    }
    std::vector<std::shared_ptr<const PointSelectionState>> selections;selections.reserve(graph.selections.size());
    for(auto &stored:graph.selections)
    {
        if(stored.entries.empty()||stored.entries.size()>MaximumPointSelections)throw std::length_error("Project point selection count limit");
        auto state=std::make_shared<PointSelectionState>();state->entries.reserve(stored.entries.size());
        for(auto &record:stored.entries)
        {
            std::shared_ptr<const std::vector<MeshPointMask>> masks;
            if(record.masks){masks=maskSets.at(*record.masks);usedMasks[*record.masks]=true;}
            state->entries.push_back({std::move(record.name),std::move(masks)});
        }
        selections.push_back(std::move(state));
    }
    if(std::any_of(usedMasks.begin(),usedMasks.end(),[](bool v){return !v;}))throw std::invalid_argument("Unreferenced project mask set");
    std::vector<bool> usedSelections(selections.size(),false);
    Object3DProject result;result.editor=engine.editObject(assets.at(graph.history[0].asset),graph.limit);auto &editor=result.editor.get();
    std::vector<Object3DEditor::Entry> history;history.reserve(graph.history.size());
    for(const auto &entry:graph.history)
    {
        const auto object=assets.at(entry.asset);std::shared_ptr<const LayerState> state;
        if(entry.layers)
        {
            state=states.at(*entry.layers);Object3DAsset rebuilt(editor.rebuild(*state));auto expected=rebuilt.data(),stored=object.get().data();
            assertEquivalentProjectAssets(expected,stored,options);
        }
        std::shared_ptr<const PointSelectionState> selection;
        if(entry.selections)
        {
            selection=selections.at(*entry.selections);
            if(usedSelections[*entry.selections])validatePointSelectionShape(object.get().data(),*selection);
            else validatePointSelectionState(object.get().data(),*selection);
            usedSelections[*entry.selections]=true;
        }
        const ProjectHistorySelectionView currentSelection{object.get().data(),selection.get(),static_cast<bool>(state)};
        if(history.empty())
            validateProjectHistorySelectionTransition(graph.formatVersion,nullptr,currentSelection,false,entry.selectionRemap);
        else
        {
            const auto &previous=history.back();
            const ProjectHistorySelectionView previousSelection{previous.object.get().data(),
                previous.selections.get(),static_cast<bool>(previous.layers)};
            validateProjectHistorySelectionTransition(graph.formatVersion,&previousSelection,currentSelection,
                object==previous.object,entry.selectionRemap);
        }
        auto remapping=entry.selectionRemap.empty()?nullptr:
            std::make_shared<const std::vector<std::optional<std::size_t>>>(entry.selectionRemap);
        history.push_back({object,entry.label,std::move(state),std::move(selection),std::move(remapping)});
    }
    if(std::any_of(usedSelections.begin(),usedSelections.end(),[](bool v){return !v;}))throw std::invalid_argument("Unreferenced project selection state");
    editor.history_=std::move(history);editor.cursor_=graph.cursor;editor.revision_=graph.revision;editor.nextLayer_=graph.nextLayer;
    result.rigs=restoreProjectRigs(engine,graph,assets,programs,options);
    if(graph.animations.size()>4096)throw std::length_error("Project animation count limit");
    const auto current=editor.object();result.animations.reserve(graph.animations.size());
    for(auto &animation:graph.animations)
    {
        auto clip=std::make_shared<AnimationClip>(current.get().skeleton(),animation.duration,
            std::move(animation.tracks),std::move(animation.curves),std::move(animation.requiredDeformationBind));
        result.animations.push_back({std::move(animation.name),current,std::move(clip),
                                     std::move(animation.morphBindings),animation.sourceStartSeconds});
    }
    object3d_detail::validateProjectAnimations(result.animations,current);
    return result;
}
} // namespace iiActorObject::object3d_detail
