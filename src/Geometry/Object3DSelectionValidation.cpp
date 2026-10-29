#include "Object3DSelectionValidation.h"
#include "Object3DData.h"
#include "Object3DNames.h"
#include "MeshPointMaskValidation.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace iiActorObject::object3d_detail
{
namespace
{
bool zero(const MeshPointMask &mask)
{return std::all_of(mask.weights.begin(),mask.weights.end(),[](double value){return value==0;});}
bool samePoints(const Object3DMesh &a,const Object3DMesh &b)
{
    if(a.topology.has_value()!=b.topology.has_value())return false;
    if(!a.topology)
        return a.geometry.vertices.size()==b.geometry.vertices.size()&&a.geometry.triangles==b.geometry.triangles;
    const auto &x=*a.topology,&y=*b.topology;
    if(!x.layout||!y.layout||x.points.size()!=y.points.size())return false;
    if(x.layout==y.layout)return true;
    const auto &left=x.layout->definition(),&right=y.layout->definition();
    if(left.sourceFormat!=right.sourceFormat||left.sourceElement!=right.sourceElement||
       left.positionCount!=right.positionCount||left.polygons.size()!=right.polygons.size())return false;
    for(std::size_t f=0;f<left.polygons.size();++f)
    {
        const auto &p=left.polygons[f],&q=right.polygons[f];if(p.size()!=q.size())return false;
        for(std::size_t c=0;c<p.size();++c)
            if(left.layouts.at(p[c]).position!=right.layouts.at(q[c]).position)return false;
    }
    return true;
}
}
void validatePointSelectionState(const Object3DData &data,const PointSelectionState &state)
{
    if(state.entries.empty()||state.entries.size()>MaximumPointSelections)
        throw std::length_error("Point selection count limit");
    std::size_t weights=0;std::string_view previous;
    for(const auto &entry:state.entries)
    {
        validateName(entry.name);
        if(!previous.empty()&&previous>=entry.name)throw std::invalid_argument("Duplicate or unordered point selection name");
        previous=entry.name;
        if(!entry.masks)continue;
        if(entry.masks->empty())throw std::invalid_argument("Noncanonical empty point selection masks");
        validateMeshPointMasks(data,*entry.masks);
        if(!std::is_sorted(entry.masks->begin(),entry.masks->end(),[](const auto &a,const auto &b){return a.mesh<b.mesh;})||
           std::any_of(entry.masks->begin(),entry.masks->end(),zero))
            throw std::invalid_argument("Noncanonical point selection masks");
        for(const auto &mask:*entry.masks)
        {
            if(mask.weights.size()>MaximumPointSelectionWeights-weights)throw std::length_error("Point selection weight budget");
            weights+=mask.weights.size();
        }
    }
}
void validatePointSelectionShape(const Object3DData &data,const PointSelectionState &state)
{
    for(const auto &entry:state.entries)if(entry.masks)for(const auto &mask:*entry.masks)
    {
        const auto &mesh=data.meshes.at(mask.mesh);
        const auto count=mesh.topology?mesh.topology->points.size():mesh.geometry.vertices.size();
        if(mask.weights.size()!=count)throw std::invalid_argument("Point selection dimensions changed");
    }
}
void validatePointSelectionTransition(const Object3DData &before,const Object3DData &after,const PointSelectionState &state)
{
    validatePointSelectionShape(after,state);std::set<std::size_t> meshes;
    for(const auto &entry:state.entries)if(entry.masks)for(const auto &mask:*entry.masks)meshes.insert(mask.mesh);
    for(const auto mesh:meshes)
        if(!samePoints(before.meshes.at(mesh),after.meshes.at(mesh)))
            throw std::invalid_argument("Point selection requires stable mesh and source point IDs");
}
void validateRemappedPointSelectionTransition(const Object3DData &before,const Object3DData &after,
    const PointSelectionState &previous,const PointSelectionState &next,
    std::span<const std::optional<std::size_t>> oldToNew,SelectionRemapMode mode)
{
    if(oldToNew.size()!=before.meshes.size()||previous.entries.size()!=next.entries.size())
        throw std::invalid_argument("Invalid point selection mesh remapping");
    std::size_t destination=0;std::optional<std::size_t> last;bool removed=false;
    for(std::size_t i=0;i<oldToNew.size();++i)
    {
        if(!oldToNew[i]){removed=true;if(mode==SelectionRemapMode::Insertion)throw std::invalid_argument("Insertion cannot discard a selected source mesh");continue;}
        const auto target=*oldToNew[i];
        if((mode==SelectionRemapMode::Removal?target!=destination:(last&&target<=*last))||target>=after.meshes.size()||
           !samePoints(before.meshes[i],after.meshes[target])||
           (mode==SelectionRemapMode::Mixed&&before.meshes[i].name!=after.meshes[target].name))
            throw std::invalid_argument("Point selection remapping changes source point identities");
        last=target;++destination;
    }
    if((mode==SelectionRemapMode::Insertion&&after.meshes.size()<=before.meshes.size())||
       (mode==SelectionRemapMode::Removal&&destination!=after.meshes.size())||
       (mode==SelectionRemapMode::Mixed&&(!removed||destination>=after.meshes.size())))
        throw std::invalid_argument("Point selection remapping omits a retained mesh");
    validatePointSelectionState(after,next);
    for(std::size_t i=0;i<previous.entries.size();++i)
    {
        const auto &a=previous.entries[i],&b=next.entries[i];
        if(a.name!=b.name)throw std::invalid_argument("Point selection names changed during remapping");
        std::vector<MeshPointMask> expected;
        if(a.masks)for(const auto &row:*a.masks)
        {
            if(row.mesh>=oldToNew.size())throw std::invalid_argument("Point selection source mesh is invalid");
            if(oldToNew[row.mesh]){auto copy=row;copy.mesh=*oldToNew[row.mesh];expected.push_back(std::move(copy));}
        }
        std::sort(expected.begin(),expected.end(),[](const auto &x,const auto &y){return x.mesh<y.mesh;});
        if((b.masks&&*b.masks!=expected)||(!b.masks&&!expected.empty()))
            throw std::invalid_argument("Point selection weights changed during remapping");
    }
}
} // namespace iiActorObject::object3d_detail
