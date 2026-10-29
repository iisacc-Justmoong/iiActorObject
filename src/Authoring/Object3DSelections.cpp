#include "Object3DEditor.h"
#include "Object3DNames.h"
#include "Object3DSelectionState.h"
#include "MeshPointMaskValidation.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace iiActorObject
{
namespace
{
using object3d_detail::PointSelectionRecord;
using object3d_detail::PointSelectionState;
auto find(std::vector<PointSelectionRecord> &entries,std::string_view name)
{return std::lower_bound(entries.begin(),entries.end(),name,[](const auto &a,std::string_view b){return a.name<b;});}
auto find(const std::vector<PointSelectionRecord> &entries,std::string_view name)
{return std::lower_bound(entries.begin(),entries.end(),name,[](const auto &a,std::string_view b){return a.name<b;});}
}
std::vector<std::string> Object3DEditor::pointSelectionNames() const
{
    (void)object().get();
    std::vector<std::string> result;
    if(const auto &state=history_[cursor_].selections)
        for(const auto &entry:state->entries)result.push_back(entry.name);
    return result;
}
std::span<const MeshPointMask> Object3DEditor::pointSelection(std::string_view name) const
{
    (void)object().get();
    const auto &state=history_[cursor_].selections;
    if(!state)throw std::out_of_range("Unknown point selection");
    const auto it=find(state->entries,name);
    if(it==state->entries.end()||it->name!=name)throw std::out_of_range("Unknown point selection");
    return it->masks?std::span<const MeshPointMask>(*it->masks):std::span<const MeshPointMask>{};
}
void Object3DEditor::publishSelections(std::shared_ptr<const PointSelectionState> selections,std::string label)
{
    (void)object().get();checkRevision();std::vector<Entry> history(history_.begin(),history_.begin()+static_cast<std::ptrdiff_t>(cursor_+1));
    history.reserve(history.size()+1);const auto &current=history_[cursor_];
    history.push_back({current.object,std::move(label),current.layers,std::move(selections),{}});
    if(history.size()>limit_+1)history.erase(history.begin());
    history_.swap(history);cursor_=history_.size()-1;++revision_;history.clear();owner_.collectGarbage();
}
void Object3DEditor::setPointSelection(std::string name,std::span<const MeshPointMask> masks)
{
    object3d_detail::validateName(name);const auto &data=object().get().data();validateMeshPointMasks(data,masks);
    std::size_t weightCount=0;
    for(const auto &mask:masks)
    {
        if(mask.weights.size()>object3d_detail::MaximumPointSelectionWeights-weightCount)throw std::length_error("Point selection weight budget");
        weightCount+=mask.weights.size();
    }
    std::vector<MeshPointMask> values(masks.begin(),masks.end());
    std::erase_if(values,[](const auto &mask){return std::all_of(mask.weights.begin(),mask.weights.end(),[](double v){return v==0;});});
    std::sort(values.begin(),values.end(),[](const auto &a,const auto &b){return a.mesh<b.mesh;});
    auto next=history_[cursor_].selections?std::make_shared<PointSelectionState>(*history_[cursor_].selections):std::make_shared<PointSelectionState>();
    auto it=find(next->entries,name);
    if(it!=next->entries.end()&&it->name==name)
    {
        if((values.empty()&&!it->masks)||(it->masks&&*it->masks==values))return;
        it->masks=values.empty()?nullptr:std::make_shared<const std::vector<MeshPointMask>>(std::move(values));
    }
    else
    {
        auto stored=values.empty()?nullptr:std::make_shared<const std::vector<MeshPointMask>>(std::move(values));
        next->entries.insert(it,{std::move(name),std::move(stored)});
    }
    object3d_detail::validatePointSelectionState(data,*next);publishSelections(std::move(next),"Set point selection");
}
Object3DSelectionTransferReport Object3DEditor::replaceWithProjectedSelections(const Object3DHandle &target,
    std::span<const MeshMaskProjectionPair> pairs,const Object3DSelectionTransferOptions &options,std::string label)
{
    const auto &original=object().get();
    if(!owner_.owns(target)||target==object())throw std::invalid_argument("Selection transfer target must be a different same-engine asset");
    const auto &destination=target.get();
    if(layered()&&!options.detachTemplate)throw std::invalid_argument("Selection transfer requires explicit template detachment");
    checkRevision();const auto current=history_[cursor_].selections;
    std::set<std::size_t> mapped;
    for(const auto &pair:pairs)mapped.insert(pair.sourceMesh);
    std::vector<MeshMaskProjectionInput> inputs;std::size_t discarded=0;
    if(current)
    {
        inputs.reserve(current->entries.size());
        for(const auto &record:current->entries)
        {
            const auto masks=record.masks?std::span<const MeshPointMask>(*record.masks):std::span<const MeshPointMask>{};
            for(const auto &row:masks)if(!mapped.contains(row.mesh))++discarded;
            inputs.push_back({masks});
        }
    }
    if(discarded&&!options.discardUnmappedSelections)
        throw std::invalid_argument("Selection transfer has unmapped source rows");
    auto projected=projectMeshPointMaskSets(original,destination,inputs,pairs,options.projection);
    std::shared_ptr<PointSelectionState> next;
    if(current)
    {
        next=std::make_shared<PointSelectionState>();next->entries.reserve(current->entries.size());
        for(std::size_t i=0;i<current->entries.size();++i)
        {
            auto &rows=projected.sets[i];
            std::erase_if(rows,[](const auto &row){return std::all_of(row.weights.begin(),row.weights.end(),[](double value){return value==0;});});
            auto stored=rows.empty()?nullptr:std::make_shared<const std::vector<MeshPointMask>>(std::move(rows));
            next->entries.push_back({current->entries[i].name,std::move(stored)});
        }
        object3d_detail::validatePointSelectionState(destination.data(),*next);
    }
    std::vector<Entry> history(history_.begin(),history_.begin()+static_cast<std::ptrdiff_t>(cursor_+1));
    history.reserve(history.size()+1);history.push_back({target,std::move(label),{},std::move(next),{}});
    if(history.size()>limit_+1)history.erase(history.begin());
    history_.swap(history);cursor_=history_.size()-1;++revision_;history.clear();owner_.collectGarbage();
    return {inputs.size(),discarded,projected.projectedPoints,projected.skippedUnusedPoints,
            projected.nodeVisits,projected.triangleTests,projected.weightEvaluations,projected.outputWeights,projected.maximumDistance};
}
void Object3DEditor::renamePointSelection(std::string_view oldName,std::string newName)
{
    (void)object().get();object3d_detail::validateName(newName);const auto &current=history_[cursor_].selections;
    if(!current)throw std::out_of_range("Unknown point selection");
    auto next=std::make_shared<PointSelectionState>(*current);auto old=find(next->entries,oldName);
    if(old==next->entries.end()||old->name!=oldName)throw std::out_of_range("Unknown point selection");
    if(oldName==newName)return;const auto other=find(next->entries,newName);
    if(other!=next->entries.end()&&other->name==newName)throw std::invalid_argument("Duplicate point selection name");
    auto record=*old;record.name=std::move(newName);next->entries.erase(old);
    const auto destination=find(next->entries,record.name);next->entries.insert(destination,std::move(record));
    publishSelections(std::move(next),"Rename point selection");
}
void Object3DEditor::removePointSelection(std::string_view name)
{
    (void)object().get();const auto &current=history_[cursor_].selections;if(!current)throw std::out_of_range("Unknown point selection");
    auto next=std::make_shared<PointSelectionState>(*current);const auto it=find(next->entries,name);
    if(it==next->entries.end()||it->name!=name)throw std::out_of_range("Unknown point selection");
    next->entries.erase(it);
    publishSelections(next->entries.empty()?nullptr:std::move(next),"Remove point selection");
}
void Object3DEditor::setLayerPointSelection(Object3DLayerId id,std::string_view name)
{
    (void)layerPointMasks(id); // Check the layer before allocating a complete mask.
    const auto selection=pointSelection(name);const auto &data=object().get().data();
    std::size_t weightCount=0;
    for(const auto &mesh:data.meshes)
    {
        const auto count=mesh.topology?mesh.topology->points.size():mesh.geometry.vertices.size();
        if(count>object3d_detail::MaximumPointSelectionWeights-weightCount)throw std::length_error("Point selection layer weight budget");weightCount+=count;
    }
    std::vector<MeshPointMask> masks;masks.reserve(data.meshes.size());
    auto selected=selection.begin();
    for(std::size_t mesh=0;mesh<data.meshes.size();++mesh)
    {
        const auto &part=data.meshes[mesh];const auto count=part.topology?part.topology->points.size():part.geometry.vertices.size();
        if(selected!=selection.end()&&selected->mesh==mesh){masks.push_back(*selected);++selected;}
        else masks.push_back({mesh,std::vector<double>(count,0)});
    }
    setLayerPointMasks(id,masks);
}
} // namespace iiActorObject
