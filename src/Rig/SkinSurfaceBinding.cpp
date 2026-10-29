#include "SkinBinding.h"
#include "SkinEnvelopeGroups.h"
#include "MeshEditGraph.h"
#include "CharacterMath.h"
#include <map>
#include <queue>
#include <set>

namespace iiActorObject
{
namespace
{
using namespace character_detail;
struct Seed final {std::uint32_t point;double distance;};
using QueueEntry=std::pair<double,std::uint32_t>;

bool better(const SkinWeight &a,const SkinWeight &b) noexcept
{return a.weight!=b.weight?a.weight>b.weight:a.bone<b.bone;}
void offer(std::vector<SkinWeight> &row,SkinWeight value,std::size_t maximum)
{
    if(row.size()<maximum){row.push_back(value);return;}
    auto worst=std::max_element(row.begin(),row.end(),better);
    if(better(value,*worst))*worst=value;
}
double score(double distance,double radius,double falloff)
{
    const auto ratio=distance/radius;
    if(!std::isfinite(ratio))throw std::overflow_error("Surface skin distance overflow");
    const auto value=-falloff*std::log1p(ratio);
    if(!std::isfinite(value))throw std::overflow_error("Surface skin score overflow");
    return value;
}
double directDistance(Vector3 position,const BoneEnvelope &envelope)
{
    const auto axis=sub(envelope.end,envelope.start);const auto extent=length(axis);
    const auto direction=extent==0?Vector3{}:mul(axis,1/extent);
    const auto along=std::clamp(dot(sub(position,envelope.start),direction),0.0,extent);
    const auto distance=length(sub(position,add(envelope.start,mul(direction,along))));
    if(!std::isfinite(distance))throw std::overflow_error("Surface skin envelope distance overflow");
    return distance;
}
void validate(const Object3DData &data,const Object3DMesh &mesh,std::span<const BoneEnvelope> envelopes,
              const SurfaceBindOptions &o)
{
    if(envelopes.empty()||o.weights.maxInfluences==0||o.weights.maxInfluences>32||
       !std::isfinite(o.weights.falloff)||o.weights.falloff<=0||o.samplesPerBone==0||o.samplesPerBone>64||
       !o.maximumPoints||!o.maximumCorners||!o.maximumSeedComparisons||!o.maximumPointVisits||
       !o.maximumFallbackComparisons||!o.maximumEdgeVisits||
       !o.maximumWeightEntries||!o.maximumQueueSize||o.unreachable<SurfaceBindUnreachable::Reject||
       o.unreachable>SurfaceBindUnreachable::Euclidean)
        throw std::invalid_argument("Invalid surface skin binding options");
    (void)Skeleton(data.bones);
    if(envelopes.size()>o.maximumWeightEntries)throw std::length_error("Surface skin envelope budget");
    const auto points=mesh.topology?mesh.topology->points.size():mesh.geometry.vertices.size();
    if(!points||points>o.maximumPoints||points>=NoTopologyIndex)throw std::length_error("Surface skin point budget");
    std::size_t corners=0;
    if(mesh.topology)
    {
        if(!mesh.topology->layout)throw std::invalid_argument("Missing surface skin topology");
        for(const auto &face:mesh.topology->layout->definition().polygons)
        {
            if(face.size()>o.maximumCorners-corners)throw std::length_error("Surface skin corner budget");
            corners+=face.size();
        }
    }
    else if(mesh.geometry.triangles.size()>o.maximumCorners)throw std::length_error("Surface skin corner budget");
    if(o.seeds.size()>o.maximumWeightEntries)throw std::length_error("Surface skin seed budget");
}
}
SurfaceBindResult SkinBinder::bindSurfaceMesh(Object3DData &data,std::size_t meshIndex,
    std::span<const BoneEnvelope> envelopes,const SurfaceBindOptions &options)
{
    const auto &mesh=data.meshes.at(meshIndex);validate(data,mesh,envelopes,options);
    const auto groups=object3d_detail::groupEnvelopes(envelopes,data.bones.size());
    const object3d_detail::MeshEditGraph graph(mesh);const auto count=graph.positions.size();
    std::vector<std::uint32_t> visiblePoints,eligiblePoints;
    for(std::size_t p=0;p<count;++p)
    {
        if(graph.visible[p])visiblePoints.push_back(static_cast<std::uint32_t>(p));
        if(graph.visible[p]||options.includeUnusedPoints)eligiblePoints.push_back(static_cast<std::uint32_t>(p));
    }
    const auto visible=visiblePoints.size();
    if(!visible&&options.seeds.empty())throw std::invalid_argument("Surface skin binding needs visible points or explicit seeds");
    const auto eligible=eligiblePoints.size();
    const auto perPoint=std::min(groups.size(),options.weights.maxInfluences);
    if(eligible>options.maximumWeightEntries/perPoint)throw std::length_error("Surface skin weight budget");
    if(count>std::numeric_limits<std::size_t>::max()-eligible ||
       count+eligible>options.maximumPointVisits/groups.size())
        throw std::length_error("Surface skin point visit budget");
    std::vector<std::vector<double>> edges(count);
    for(std::size_t p=0;p<count;++p)
    {
        edges[p].reserve(graph.neighbours[p].size());
        for(const auto neighbour:graph.neighbours[p])
        {
            const auto distance=length(sub(graph.positions[p],graph.positions[neighbour]));
            if(!std::isfinite(distance))throw std::overflow_error("Surface skin edge overflow");
            edges[p].push_back(distance);
        }
    }
    std::vector<std::vector<Seed>> seeds(groups.size());SurfaceBindResult report;report.pointVisits=(count+eligible)*groups.size();
    if(options.seeds.empty())
    {
        std::size_t seedCount=0;
        for(std::size_t i=0;i<groups.size();++i)
        {
          for(const auto &e:groups[i].segments)
          {
            const auto samples=e.start==e.end?std::size_t{1}:options.samplesPerBone;
            if(samples>options.maximumWeightEntries-seedCount)throw std::length_error("Surface skin seed budget");
            seedCount+=samples;
            if(visible>0&&samples>(options.maximumSeedComparisons-report.seedComparisons)/visible)
                throw std::length_error("Surface skin seed comparison budget");
            for(std::size_t sample=0;sample<samples;++sample)
            {
                const double t=samples==1?.5:static_cast<double>(sample)/static_cast<double>(samples-1);
                const Vector3 target{std::lerp(e.start.x,e.end.x,t),std::lerp(e.start.y,e.end.y,t),std::lerp(e.start.z,e.end.z,t)};
                double nearest=std::numeric_limits<double>::infinity();std::uint32_t point=NoTopologyIndex;
                for(const auto p:visiblePoints)
                {
                    ++report.seedComparisons;const auto distance=length(sub(graph.positions[p],target));
                    if(!std::isfinite(distance))throw std::overflow_error("Surface skin seed distance overflow");
                    if(distance<nearest){nearest=distance;point=static_cast<std::uint32_t>(p);}
                }
                if(point==NoTopologyIndex)throw std::invalid_argument("No visible surface seed");
                seeds[i].push_back({point,nearest});
            }
          }
        }
    }
    else
    {
        std::map<BoneIndex,std::size_t> byBone;for(std::size_t i=0;i<groups.size();++i)byBone.emplace(groups[i].bone,i);
        std::set<std::pair<BoneIndex,std::uint32_t>> seen;
        for(const auto &seed:options.seeds)
        {
            const auto found=byBone.find(seed.bone);
            if(found==byBone.end()||seed.point>=count||(!options.includeUnusedPoints&&!graph.visible[seed.point])||
               !std::isfinite(seed.distance)||seed.distance<0||!seen.emplace(seed.bone,seed.point).second)
                throw std::invalid_argument("Invalid or duplicate surface skin seed");
            seeds[found->second].push_back({seed.point,seed.distance});
        }
        for(const auto &row:seeds)if(row.empty())throw std::invalid_argument("Each surface skin bone needs a seed");
    }
    std::vector<std::vector<SkinWeight>> rows(count);
    for(std::size_t i=0;i<groups.size();++i)
    {
        const auto &group=groups[i];std::vector<double> distance(count,std::numeric_limits<double>::infinity());
        std::priority_queue<QueueEntry,std::vector<QueueEntry>,std::greater<>> pending;
        const auto push=[&](double value,std::uint32_t point)
        {
            if(value>=distance[point])return;
            if(pending.size()>=options.maximumQueueSize)throw std::length_error("Surface skin queue budget");
            distance[point]=value;pending.emplace(value,point);
        };
        for(const auto seed:seeds[i])push(seed.distance,seed.point);
        while(!pending.empty())
        {
            const auto [value,p]=pending.top();pending.pop();if(value!=distance[p])continue;
            for(std::size_t edge=0;edge<graph.neighbours[p].size();++edge)
            {
                if(report.edgeVisits==options.maximumEdgeVisits)throw std::length_error("Surface skin edge visit budget");
                ++report.edgeVisits;const auto candidate=value+edges[p][edge];
                if(!std::isfinite(candidate))throw std::overflow_error("Surface skin path overflow");
                push(candidate,graph.neighbours[p][edge]);
            }
        }
        for(const auto p:eligiblePoints)if(std::isfinite(distance[p]))
            offer(rows[p],{group.bone,score(distance[p],group.radius,options.weights.falloff)},options.weights.maxInfluences);
    }
    for(std::size_t p=0;p<count;++p)
    {
        if(!graph.visible[p]&&!options.includeUnusedPoints){++report.excludedPoints;continue;}
        auto &row=rows[p];
        if(row.empty())
        {
            ++report.unreachablePoints;
            if(options.unreachable==SurfaceBindUnreachable::Reject)throw std::out_of_range("Surface skin point has no reachable bone seed");
            if(options.unreachable==SurfaceBindUnreachable::Keep)continue;
            if(envelopes.size()>options.maximumFallbackComparisons-report.fallbackComparisons)
                throw std::length_error("Surface skin fallback comparison budget");
            report.fallbackComparisons+=envelopes.size();
            for(const auto &group:groups)
            {
                double distance=std::numeric_limits<double>::infinity();
                for(const auto &e:group.segments)distance=std::min(distance,directDistance(graph.positions[p],e));
                offer(row,{group.bone,score(distance,group.radius,options.weights.falloff)},options.weights.maxInfluences);
            }
            ++report.fallbackPoints;
        }
        std::sort(row.begin(),row.end(),better);const auto largest=row.front().weight;double total=0;
        for(auto &w:row){w.weight=std::exp(w.weight-largest);total+=w.weight;}
        for(auto &w:row)w.weight/=total;++report.boundPoints;
    }
    auto result=data;auto &target=result.meshes.at(meshIndex);
    for(std::size_t p=0;p<count;++p)if(!rows[p].empty())
    {
        if(target.topology)target.topology->points[p].weights=std::move(rows[p]);
        else target.geometry.vertices[p].weights=std::move(rows[p]);
    }
    if(target.topology)
    {
        auto render=renderMeshTopology(*target.topology);target.geometry=std::move(render.geometry);
        target.textureCoordinates=std::move(render.textureCoordinates);target.triangleMaterials=std::move(render.triangleMaterials);
        target.primaryUvName=std::move(render.primaryUvName);target.additionalUvSets=std::move(render.additionalUvSets);
    }
    data=std::move(result);return report;
}
} // namespace iiActorObject
