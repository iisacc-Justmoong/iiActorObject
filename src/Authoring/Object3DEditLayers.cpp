#include "Object3DAdjustment.h"
#include "CharacterMath.h"

namespace iiActorObject
{
Object3DExternalReport Object3DEditor::applyExternalEdits(const Object3DAsset &source,Object3DExternalSnapshot baseline,Object3DExternalSnapshot edited,const Object3DExternalOptions &options)
{
    const std::string label="Import external edits";
    if(!layered())
    {
        if(options.replaceLayer)throw std::logic_error("External layer replacement requires a template editor");
        auto data=object().get().data();auto report=applyObject3DExternalEdits(data,source,baseline,edited,options);
        if(report.changed())commit(std::move(data),label);return report;
    }
    auto state=copyLayers();std::size_t replace=state->layers.size();
    if(options.replaceLayer){const auto found=std::find_if(state->layers.begin(),state->layers.end(),[&](const auto &v){return v.info.id==*options.replaceLayer;});if(found==state->layers.end())throw std::out_of_range("External replacement layer");replace=found-state->layers.begin();}
    auto prefix=*state;prefix.layers.resize(replace);auto before=rebuild(prefix),desired=before;auto report=applyObject3DExternalEdits(desired,source,baseline,edited,options);
    if(!report.changed()&&!options.replaceLayer)return report;
    if(!options.replaceLayer&&(state->layers.size()>=4096||nextLayer_==std::numeric_limits<Object3DLayerId>::max()))throw std::length_error("External edit layer limit");
    const auto &definition=state->program.get().definition();const auto &bindings=definition.surfaces;
    const auto evaluation=object3d_detail::layerEvaluationFor(definition);
    const bool existingNormals=std::any_of(prefix.layers.begin(),prefix.layers.end(),[](const auto &layer)
        {return layer.info.enabled&&std::any_of(layer.adjustment->meshes.begin(),layer.adjustment->meshes.end(),
            [](const auto &mesh){return !mesh.normals.empty();});});
    const bool needsNormalPrediction=report.normals||(report.positions&&existingNormals);
    const auto finalDesired=needsNormalPrediction?std::optional<Object3DData>(desired):std::nullopt;
    if(!bindings.empty())
    {
        if(evaluation==Object3DLayerEvaluation::EvaluatedSurface)unapplyMeshSurfaceBindingsWithBases(desired,before,bindings,definition.surfaceOffsetBases);
        else{const auto reference=state->program.get().evaluate(state->values);unapplyMeshSurfaceBindingsWithBases(before,reference,bindings,definition.surfaceOffsetBases);unapplyMeshSurfaceBindingsWithBases(desired,reference,bindings,definition.surfaceOffsetBases);}
    }
    auto adjustment=std::make_shared<object3d_detail::Adjustment>(object3d_detail::difference(before,desired));
    if(needsNormalPrediction)
    {
        object3d_detail::clearNormalAdjustments(*adjustment);
        auto prediction=prefix;prediction.layers.push_back({{nextLayer_,label,true,1,evaluation},adjustment});
        const auto predicted=rebuild(prediction);
        const auto keys=object3d_detail::changedNormals(predicted,*finalDesired);
        object3d_detail::setNormalAdjustments(*adjustment,predicted,*finalDesired,keys);
    }
    if(options.replaceLayer){state->layers[replace].adjustment=std::move(adjustment);state->layers[replace].info.evaluation=evaluation;report.layer=state->layers[replace].info.id;}
    else{state->layers.push_back({{nextLayer_,label,true,1,evaluation},std::move(adjustment)});report.layer=nextLayer_;}
    auto result=rebuild(*state);publish(std::move(result),label,std::move(state));if(!options.replaceLayer)++nextLayer_;report.applied=true;return report;
}
namespace
{
std::vector<TemplateValue> canonical(std::span<const TemplateValue> values)
{
    std::vector<TemplateValue> result(values.begin(),values.end());
    std::erase_if(result,[](const auto &v){return v.value==0;});
    std::sort(result.begin(),result.end(),[](const auto &a,const auto &b){return a.name<b.name;});return result;
}
std::size_t indexOf(const object3d_detail::LayerState &state,Object3DLayerId id)
{
    for(std::size_t i=0;i<state.layers.size();++i)if(state.layers[i].info.id==id)return i;
    throw std::out_of_range("Unknown 3D edit layer");
}
}
Object3DEditor::Object3DEditor(Object3DReference owner,const Object3DTemplateHandle &program,
    std::span<const TemplateValue> values,std::size_t historyLimit):owner_(std::move(owner)),limit_(historyLimit)
{
    if(limit_==0||limit_>4096||!owner_.owns(program.get().definition().base))
        throw std::invalid_argument("Template editor requires a source from its owning engine and valid history limit");
    // Validate the complete input before removing zero values or sorting it.
    auto data=program.get().evaluate(values);
    auto state=std::make_shared<object3d_detail::LayerState>();state->program=program;state->values=canonical(values);state->referencePose=data.referencePose;
    if(!program.get().definition().fittings.empty())data=applyLayers(std::move(data),*state);
    (void)object3d_detail::difference(data,data); // Stable unit-scale editing domain.
    history_.reserve(1);auto object=owner_.createObject(std::move(data));
    history_.push_back({std::move(object),"Template source",std::move(state),{}, {}});
}
bool Object3DEditor::layered() const noexcept { return static_cast<bool>(history_[cursor_].layers); }
Object3DTemplateHandle Object3DEditor::templateProgram() const
{return layered()?history_[cursor_].layers->program:Object3DTemplateHandle{};}
std::span<const TemplateValue> Object3DEditor::templateValues() const noexcept
{
    const auto &state=history_[cursor_].layers;
    return state?std::span<const TemplateValue>(state->values):std::span<const TemplateValue>{};
}
std::vector<Object3DEditLayerInfo> Object3DEditor::layers() const
{
    std::vector<Object3DEditLayerInfo> result;
    if(const auto &state=history_[cursor_].layers)for(const auto &layer:state->layers)result.push_back(layer.info);
    return result;
}
std::shared_ptr<object3d_detail::LayerState> Object3DEditor::copyLayers() const
{
    if(!layered())throw std::logic_error("This editing session is not bound to a template");
    return std::make_shared<object3d_detail::LayerState>(*history_[cursor_].layers);
}
Object3DData Object3DEditor::rebuild(const object3d_detail::LayerState &state) const
{
    return applyLayers(state.program.get().evaluate(state.values),state);
}
Object3DData Object3DEditor::applyLayers(Object3DData data,const object3d_detail::LayerState &state) const
{
    std::vector<std::uint8_t> moved(data.meshes.size(),0);
    const auto &definition=state.program.get().definition();const auto &surfaces=definition.surfaces;std::optional<Object3DData> surfaceReference;
    if(!surfaces.empty())surfaceReference=data;
    bool evaluated=false,dirty=false;
    auto evaluateSurfaces=[&]
    {
        if(evaluated&&!dirty)return;
        if(surfaceReference)applyMeshSurfaceBindingsWithBases(data,*surfaceReference,surfaces,definition.surfaceOffsetBases,moved);
        (void)applyObject3DSurfaceFittingWithBases(data,surfaces,definition.fittings,definition.surfaceOffsetBases);
        if(surfaceReference)surfaceReference=data;
        evaluated=true;dirty=false;
    };
    for(const auto &layer:state.layers)
        if(layer.info.enabled&&layer.info.weight!=0)
        {
            if(layer.info.evaluation==Object3DLayerEvaluation::EvaluatedSurface)evaluateSurfaces();
            else if(layer.info.evaluation!=Object3DLayerEvaluation::Deferred)throw std::invalid_argument("Unknown edit layer evaluation");
            object3d_detail::applySurfaces(data,*layer.adjustment,layer.info.weight,moved,
                layer.pointMasks?std::span<const MeshPointMask>(*layer.pointMasks):std::span<const MeshPointMask>{});
            dirty|=std::any_of(layer.adjustment->meshes.begin(),layer.adjustment->meshes.end(),[](const auto &mesh){return !mesh.positions.empty();});
        }
    evaluateSurfaces();
    bindObject3DTemplateJoints(data,state.program.get().definition().joints);
    for(const auto &layer:state.layers)
        if(layer.info.enabled)object3d_detail::applyJoints(data,*layer.adjustment,layer.info.weight);
    object3d_detail::refreshSurfaces(data,moved);
    for(const auto &layer:state.layers)
        if(layer.info.enabled&&layer.info.weight!=0)
            object3d_detail::applyNormalSurfaces(data,*layer.adjustment,layer.info.weight,
                layer.pointMasks?std::span<const MeshPointMask>(*layer.pointMasks):std::span<const MeshPointMask>{});
    data.referencePose=state.referencePose;return data;
}
void Object3DEditor::setTemplateValues(std::span<const TemplateValue> values)
{
    auto state=copyLayers();
    auto data=state->program.get().evaluate(values); // Reject unknown/duplicate zero-valued controls too.
    auto next=canonical(values);if(next==state->values)return;state->values=std::move(next);
    data=applyLayers(std::move(data),*state);publish(std::move(data),"Set template parameters",std::move(state));
}
void Object3DEditor::setLayerEnabled(Object3DLayerId id,bool enabled)
{
    auto state=copyLayers();auto &layer=state->layers[indexOf(*state,id)];if(layer.info.enabled==enabled)return;
    layer.info.enabled=enabled;auto data=rebuild(*state);publish(std::move(data),"Enable edit layer",std::move(state));
}
void Object3DEditor::setLayerWeight(Object3DLayerId id,double weight)
{
    character_detail::weight(weight);auto state=copyLayers();auto &layer=state->layers[indexOf(*state,id)];
    if(layer.info.weight==weight)return;layer.info.weight=weight;
    auto data=rebuild(*state);publish(std::move(data),"Set edit layer weight",std::move(state));
}
void Object3DEditor::renameLayer(Object3DLayerId id,std::string label)
{
    if(label.empty()||label.size()>4096)throw std::invalid_argument("Invalid edit layer label");
    auto state=copyLayers();auto &layer=state->layers[indexOf(*state,id)];if(layer.info.label==label)return;
    layer.info.label=std::move(label);auto data=object().get().data();publish(std::move(data),"Rename edit layer",std::move(state));
}
std::span<const MeshPointMask> Object3DEditor::layerPointMasks(Object3DLayerId id) const
{
    if(!layered())throw std::logic_error("Point masks require a template layer");const auto &state=*history_[cursor_].layers;
    const auto &masks=state.layers[indexOf(state,id)].pointMasks;return masks?std::span<const MeshPointMask>(*masks):std::span<const MeshPointMask>{};
}
void Object3DEditor::setLayerPointMasks(Object3DLayerId id,std::span<const MeshPointMask> masks)
{
    auto state=copyLayers();auto &layer=state->layers[indexOf(*state,id)];validateMeshPointMasks(object().get().data(),masks);
    std::vector<MeshPointMask> next(masks.begin(),masks.end());std::erase_if(next,[](const auto &m){return std::all_of(m.weights.begin(),m.weights.end(),[](double v){return v==1;});});
    std::sort(next.begin(),next.end(),[](const auto &a,const auto &b){return a.mesh<b.mesh;});
    if((next.empty()&&!layer.pointMasks)||(layer.pointMasks&&*layer.pointMasks==next))return;
    layer.pointMasks=next.empty()?nullptr:std::make_shared<const std::vector<MeshPointMask>>(std::move(next));
    auto data=rebuild(*state);publish(std::move(data),"Set edit layer point masks",std::move(state));
}
void Object3DEditor::moveLayer(Object3DLayerId id,std::size_t index)
{
    auto state=copyLayers();const auto from=indexOf(*state,id);
    if(index>=state->layers.size())throw std::out_of_range("Edit layer destination");if(from==index)return;
    auto layer=std::move(state->layers[from]);state->layers.erase(state->layers.begin()+static_cast<std::ptrdiff_t>(from));
    state->layers.insert(state->layers.begin()+static_cast<std::ptrdiff_t>(index),std::move(layer));
    auto data=rebuild(*state);publish(std::move(data),"Reorder edit layers",std::move(state));
}
void Object3DEditor::removeLayer(Object3DLayerId id)
{
    auto state=copyLayers();const auto index=indexOf(*state,id);
    state->layers.erase(state->layers.begin()+static_cast<std::ptrdiff_t>(index));
    auto data=rebuild(*state);publish(std::move(data),"Remove edit layer",std::move(state));
}
void Object3DEditor::detachTemplate()
{
    if(!layered())return;auto data=object().get().data();publish(std::move(data),"Detach template",{});
}
std::span<const SurfaceFittingRule> Object3DEditor::surfaceFittings() const
{return layered()?std::span<const SurfaceFittingRule>(history_[cursor_].layers->program.get().definition().fittings):std::span<const SurfaceFittingRule>{};}
void Object3DEditor::setSurfaceFittings(std::span<const SurfaceFittingRule> rules)
{
    const auto existing=surfaceFittings();if(std::equal(existing.begin(),existing.end(),rules.begin(),rules.end()))return;
    auto state=layered()?copyLayers():std::make_shared<object3d_detail::LayerState>();Object3DTemplateDefinition definition;
    if(!layered())state->referencePose=object().get().data().referencePose;
    if(layered())definition=state->program.get().definition();else definition.base=object();
    definition.fittings.assign(rules.begin(),rules.end());Object3DTemplateHandle program;
    try
    {
        program=owner_.createTemplate(std::move(definition));state->program=program;auto data=rebuild(*state);
        publish(std::move(data),"Set automatic surface fitting",std::move(state));
    }
    catch(...){if(program.valid())owner_.eraseTemplate(program.id());throw;}
}
SurfaceAttachmentResult Object3DEditor::attachSurface(Object3DMesh surface,std::size_t sourceMesh,const SurfaceAttachmentOptions &options)
{
    auto current=object().get().data();(void)current.meshes.at(sourceMesh);
    auto state=layered()?copyLayers():std::make_shared<object3d_detail::LayerState>();
    if(!layered())state->referencePose=current.referencePose;
    Object3DTemplateDefinition definition;if(layered())definition=state->program.get().definition();else definition.base=object();
    const auto target=current.meshes.size();if(surface.name.empty())surface.name="Surface"+std::to_string(target);
    if(options.transferSkin&&!options.skin.skipUnmatched)
    {
        if(surface.topology)for(auto &p:surface.topology->points)p.weights.clear();
        else for(auto &v:surface.geometry.vertices)v.weights.clear();
    }
    current.meshes.push_back(std::move(surface));SurfaceAttachmentResult result{target,{}};
    if(options.transferSkin)result.skin=SkinBinder::transfer(current,sourceMesh,current,target,options.skin);
    const auto binding=bindMeshSurface(current,sourceMesh,target,options.binding);
    auto neutral=definition.base.get().data();neutral.meshes.push_back(current.meshes[target]);
    applyMeshSurfaceBindings(neutral,current,std::span(&binding,1));definition.surfaces.push_back(binding);
    if(!definition.surfaceOffsetBases.empty())definition.surfaceOffsetBases.push_back(std::nullopt);
    (void)object3d_detail::difference(neutral,neutral);
    Object3DHandle base;Object3DTemplateHandle program;
    try
    {
        base=owner_.createObject(std::move(neutral));definition.base=base;program=owner_.createTemplate(std::move(definition));state->program=program;
        auto rebuilt=rebuild(*state);publish(std::move(rebuilt),"Attach surface and skin",std::move(state));
    }
    catch(...){if(program.valid())owner_.eraseTemplate(program.id());if(base.valid())owner_.eraseObject(base.id());throw;}
    return result;
}
} // namespace iiActorObject
