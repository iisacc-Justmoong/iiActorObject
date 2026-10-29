#include "Object3DTemplateSurfaceEditRemap.h"
#include "Object3DAdjustmentData.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail
{
TemplateSurfaceEditRemapper::TemplateSurfaceEditRemapper(std::span<const std::optional<std::size_t>> mapping,
    const std::set<std::size_t> &removed,bool discardRemoved,TemplateSurfaceRemapMode mode)
    :mapping_(mapping),removed_(removed),discardRemoved_(discardRemoved),mode_(mode)
{
    if(mode!=TemplateSurfaceRemapMode::Change&&mode!=TemplateSurfaceRemapMode::Removal)
        throw std::invalid_argument("Unknown template surface edit remap mode");
}
std::size_t TemplateSurfaceEditRemapper::retained(std::size_t old) const
{
    if(old>=mapping_.size()||!mapping_[old])throw std::invalid_argument("Removed surface is required by another template feature");
    return *mapping_[old];
}
bool TemplateSurfaceEditRemapper::changesRemovalIndex(std::size_t old) const
{
    if(old>=mapping_.size())throw std::out_of_range("Template surface edit index out of range");
    return !mapping_[old]||*mapping_[old]!=old;
}
TemplateSurfaceMaskRemapResult TemplateSurfaceEditRemapper::masks(
    const std::shared_ptr<const std::vector<MeshPointMask>> &source,TemplateSurfaceMaskKind kind) const
{
    if(kind!=TemplateSurfaceMaskKind::Layer&&kind!=TemplateSurfaceMaskKind::NamedSelection)
        throw std::invalid_argument("Unknown template surface mask kind");
    if(!source)return {source,false};
    const auto message=kind==TemplateSurfaceMaskKind::Layer?
        "Removed surface has layer masks":"Removed surface has a named point selection";
    bool changed=false;
    if(mode_==TemplateSurfaceRemapMode::Change)
    {
        for(const auto &row:*source)if(removed_.contains(row.mesh)&&!discardRemoved_)throw std::invalid_argument(message);
        for(const auto &row:*source)
        {
            if(removed_.contains(row.mesh)){changed=true;continue;}
            changed|=retained(row.mesh)!=row.mesh;
        }
    }
    else
    {
        for(const auto &row:*source)
        {
            if(removed_.contains(row.mesh)&&!discardRemoved_)throw std::invalid_argument(message);
            changed|=changesRemovalIndex(row.mesh);
        }
    }
    if(!changed)return {source,false};
    std::vector<MeshPointMask> rows;rows.reserve(source->size());
    for(const auto &row:*source)if(!removed_.contains(row.mesh))
    {auto copy=row;copy.mesh=retained(row.mesh);rows.push_back(std::move(copy));}
    if(rows.empty())return {{},true};
    return {std::make_shared<const std::vector<MeshPointMask>>(std::move(rows)),true};
}
TemplateSurfaceAdjustmentRemapResult TemplateSurfaceEditRemapper::adjustment(const std::shared_ptr<const Adjustment> &source) const
{
    if(!source)throw std::invalid_argument("Missing template surface adjustment");
    bool changed=false;
    if(mode_==TemplateSurfaceRemapMode::Change)
    {
        for(const auto &row:source->meshes)if(removed_.contains(row.mesh)&&!discardRemoved_)
            throw std::invalid_argument("Removed surface has edit layers");
        changed=source->materials.has_value()||source->images.has_value()||
            std::any_of(source->meshes.begin(),source->meshes.end(),[&](const auto &row)
            {return removed_.contains(row.mesh)||retained(row.mesh)!=row.mesh;});
    }
    else
    {
        for(const auto &row:source->meshes)
        {
            if(removed_.contains(row.mesh)&&!discardRemoved_)throw std::invalid_argument("Removed surface has edit layers");
            changed|=changesRemovalIndex(row.mesh);
        }
    }
    if(!changed)return {source,{}};
    auto copy=std::make_shared<Adjustment>(*source);
    std::erase_if(copy->meshes,[&](const auto &row){return removed_.contains(row.mesh);});
    for(auto &row:copy->meshes)row.mesh=retained(row.mesh);
    return {copy,std::move(copy)};
}
} // namespace iiActorObject::object3d_detail
