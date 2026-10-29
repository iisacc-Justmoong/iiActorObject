#include "Object3DTemplateAdaptFollowers.h"
#include "SurfaceAttachment.h"
#include "CharacterMath.h"
#include <algorithm>
#include <map>
#include <queue>
#include <set>

namespace iiActorObject::object3d_detail
{
namespace
{
using namespace character_detail;
double component(Vector3 value,std::size_t axis){return axis==0?value.x:axis==1?value.y:value.z;}
}
AdaptedFollowers makeAdaptedFollowers(Object3DData &data,const Object3DTemplateDefinition &source,
    std::span<const AdaptedMeshInfo> meshes,std::span<const AdaptedHelperInfo> helpers,Vector3 scale,double yawRadians,
    const TemplateAdaptOptions &options,SkinningProfileTransfer followerProfile,TemplateAdaptReport &report)
{
    const auto transform=[&](Vector3 value)
    {
        const auto c=std::cos(yawRadians),s=std::sin(yawRadians);
        const Vector3 result{(c*value.x+s*value.z)*scale.x,value.y*scale.y,(-s*value.x+c*value.z)*scale.z};
        if(!finite(result))throw std::overflow_error("Adapted follower offset overflow");return result;
    };
    std::map<std::size_t,AdaptedMeshInfo> mapped;
    for(const auto mesh:meshes)if(!mapped.emplace(mesh.source,mesh).second)throw std::invalid_argument("Ambiguous adapted follower source");
    std::multimap<std::pair<std::size_t,std::uint32_t>,std::uint32_t> helperIds;
    for(const auto helper:helpers)helperIds.emplace(std::pair{helper.sourceMesh,helper.sourcePoint},helper.targetPoint);
    std::vector<SurfaceBinding> candidates;std::vector<std::optional<SurfaceBindingBasis>> candidateBases;
    std::vector<bool> skinTransfers;std::set<std::size_t> targets;
    const auto add=[&](SurfaceBinding binding,bool transferSkin,std::optional<SurfaceBindingBasis> basis=std::nullopt)
    {
        if(candidates.size()>=options.maximumFollowers||binding.points.size()>options.maximumFollowerPoints-report.followerPoints)
            throw std::length_error("Adapted follower budget");
        if(!targets.insert(binding.targetMesh).second)throw std::invalid_argument("Duplicate adapted follower target");
        report.followerPoints+=binding.points.size();candidates.push_back(std::move(binding));candidateBases.push_back(std::move(basis));skinTransfers.push_back(transferSkin);
    };
    const auto geometric=[&](std::size_t parent,std::size_t child)
    {
        auto limits=options.followerLimits;
        if(limits.additionalPoints.size()>limits.maximumPoints)throw std::length_error("Additional follower point budget");
        std::vector<std::uint32_t> additional(limits.additionalPoints.begin(),limits.additionalPoints.end());
        for(const auto helper:helpers)
        {
            const auto found=mapped.find(helper.sourceMesh);
            if(found!=mapped.end()&&found->second.target==child)additional.push_back(helper.targetPoint);
        }
        limits.additionalPoints=additional;
        return bindMeshSurface(data,parent,child,options.followerBinding,limits);
    };
    if(options.preserveFollowers)for(std::size_t bindingIndex=0;bindingIndex<source.surfaces.size();++bindingIndex)
    {
        const auto &binding=source.surfaces[bindingIndex];
        const auto parent=mapped.find(binding.sourceMesh),child=mapped.find(binding.targetMesh);
        if(parent==mapped.end()||child==mapped.end()){++report.skippedFollowers;continue;}
        if(binding.mode==SurfaceBindingMode::ControlPoints&&child->second.identicalPoints)
        {
            auto copied=binding;copied.sourceMesh=parent->second.target;copied.targetMesh=child->second.target;
            copied.sourceVertexCount=data.meshes.at(copied.sourceMesh).topology->points.size();
            copied.targetPointCount=data.meshes.at(copied.targetMesh).topology->points.size();
            const auto remap=[&](std::uint32_t id)
            {
                if(parent->second.identicalPoints)return parent->second.targetPoint(id);
                const auto found=helperIds.find({binding.sourceMesh,id});
                if(found==helperIds.end())throw std::logic_error("Missing projected follower anchor");return found->second;
            };
            const bool hasSpans=std::any_of(copied.offsetScales.begin(),copied.offsetScales.end(),
                                            [](const auto &axis){return axis.has_value();});
            auto offsetBasis=source.surfaceOffsetBases.empty()?std::optional<SurfaceBindingBasis>{}:source.surfaceOffsetBases.at(bindingIndex);
            const bool useBasis=offsetBasis.has_value()||(yawRadians!=0&&hasSpans);
            if(useBasis)
            {
                auto basis=offsetBasis.value_or(SurfaceBindingBasis{Vector3{1,0,0},Vector3{0,1,0},Vector3{0,0,1}});
                for(auto &column:basis)column=transform(column);offsetBasis=basis;
            }
            for(std::size_t axis=0;axis<3;++axis)if(copied.offsetScales[axis])
            {
                copied.offsetScales[axis]->a=remap(copied.offsetScales[axis]->a);
                copied.offsetScales[axis]->b=remap(copied.offsetScales[axis]->b);
                if(!useBasis)copied.offsetScales[axis]->referenceSpan*=component(scale,axis);
            }
            std::vector<std::uint32_t> originalChildPoints;originalChildPoints.reserve(copied.points.size());
            for(auto &point:copied.points)
            {
                originalChildPoints.push_back(point.point);
                point.point=child->second.targetPoint(point.point);
                for(std::size_t i=0;i<point.vertices.size();++i)
                    point.vertices[i]=parent->second.identicalPoints||point.barycentric[i]!=0?remap(point.vertices[i]):0;
                if(!useBasis)point.frameOffset=transform(point.frameOffset);
            }
            const auto originalCount=copied.points.size();
            for(std::size_t i=0;i<originalCount;++i)
            {
                const auto [first,last]=helperIds.equal_range({binding.targetMesh,originalChildPoints[i]});
                for(auto it=first;it!=last;++it){auto point=copied.points[i];point.point=it->second;copied.points.push_back(point);}
            }
            add(std::move(copied),false,std::move(offsetBasis));
            if(parent->second.identicalPoints)++report.exactFollowers;else ++report.projectedAnchorFollowers;
        }
        else
        {
            add(geometric(parent->second.target,child->second.target),false);++report.projectedFollowers;
        }
    }
    for(const auto follower:options.followers)
    {
        if(follower.sourceMesh>=data.meshes.size()||follower.targetMesh>=data.meshes.size())
            throw std::out_of_range("Adapted follower mesh");
        add(geometric(follower.sourceMesh,follower.targetMesh),follower.transferSkin);++report.explicitFollowers;
    }
    validateMeshSurfaceBindingsWithBases(data,candidates,candidateBases);
    std::vector<std::vector<std::size_t>> outgoing(data.meshes.size());std::vector<std::size_t> indegree(data.meshes.size());
    for(std::size_t i=0;i<candidates.size();++i)
    {const auto &b=candidates[i];outgoing[b.sourceMesh].push_back(i);++indegree[b.targetMesh];}
    std::queue<std::size_t> ready;for(std::size_t i=0;i<indegree.size();++i)if(!indegree[i])ready.push(i);
    AdaptedFollowers ordered;ordered.bindings.reserve(candidates.size());ordered.bases.reserve(candidates.size());
    while(!ready.empty())
    {
        const auto mesh=ready.front();ready.pop();
        for(const auto i:outgoing[mesh])
        {
            auto &candidate=candidates[i];
            if(skinTransfers[i])
            {
                const auto skin=transferMeshBindingWeights(data,candidate,0,NoBone,followerProfile);
                ++report.followerSkinMeshes;report.followerSkinPoints+=skin.transferred;
            }
            const auto child=candidate.targetMesh;ordered.bindings.push_back(std::move(candidate));ordered.bases.push_back(std::move(candidateBases[i]));
            if(--indegree[child]==0)ready.push(child);
        }
    }
    if(ordered.bindings.size()!=candidates.size())throw std::invalid_argument("Cyclic adapted follower graph");
    if(std::none_of(ordered.bases.begin(),ordered.bases.end(),[](const auto &basis){return basis.has_value();}))ordered.bases.clear();
    return ordered;
}
void neutralizeAdaptedFollowers(Object3DData &data,const Object3DData &baseline,
    std::span<const SurfaceBinding> followers,std::span<const std::optional<SurfaceBindingBasis>> bases,std::vector<bool> &moved)
{
    if(moved.size()!=data.meshes.size())throw std::invalid_argument("Adapted follower movement dimensions");
    if(!bases.empty()&&bases.size()!=followers.size())throw std::invalid_argument("Adapted follower basis count");
    for(std::size_t index=0;index<followers.size();++index)
    {
        const auto &binding=followers[index];const auto *basis=bases.empty()||!bases[index]?nullptr:&*bases[index];
        const auto target=basis?evaluateMeshSurfaceBindingWithBasis(baseline,binding,*basis):evaluateMeshSurfaceBinding(baseline,binding);
        const auto neutral=basis?evaluateMeshSurfaceBindingWithBasis(data,binding,*basis):evaluateMeshSurfaceBinding(data,binding);
        auto &mesh=data.meshes.at(binding.targetMesh);
        for(std::size_t i=0;i<binding.points.size();++i)
        {
            const auto delta=sub(target[i],neutral[i]);if(delta==Vector3{})continue;
            auto &point=mesh.topology?mesh.topology->points.at(binding.points[i].point).position:
                mesh.geometry.vertices.at(binding.points[i].point).position;
            point=sub(point,delta);if(!finite(point))throw std::overflow_error("Adapted follower neutralization overflow");
            moved[binding.targetMesh]=true;
        }
    }
}
} // namespace iiActorObject::object3d_detail
