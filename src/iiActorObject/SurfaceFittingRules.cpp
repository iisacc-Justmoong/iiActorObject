#include "SurfaceFittingRules.h"
#include "CharacterMath.h"
#include "MeshEditGraph.h"
#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <set>
#include <tuple>
#include <type_traits>

namespace iiActorObject
{
using namespace character_detail;
namespace
{
auto parameters(const SurfaceFitOptions &v)
{
    return std::tie(v.clearance,v.tolerance,v.relaxation,v.edgeStiffness,v.maximumDisplacement,v.iterations,
        v.includeUnusedPoints,v.checkTriangles,v.allowUnresolved,v.maximumPoints,v.maximumPointVisits,
        v.maximumTriangles,v.maximumNodeVisits,v.maximumTriangleTests,v.escapeConcavities,v.fairing);
}
auto colliderParameters(const MeshColliderOptions &v)
{return std::tie(v.weldRenderSeams,v.minimumRelativeVolume,v.maximumPoints,v.maximumTriangles,v.maximumComponents,v.maximumPreprocessVisits);}
constexpr auto Missing=std::numeric_limits<std::size_t>::max();
void charge(std::size_t amount,std::size_t limit,std::size_t &used)
{if(amount>limit-used)throw std::length_error("Automatic fitting aggregate collider budget");used+=amount;}
std::size_t points(const Object3DMesh &m){return m.topology?m.topology->points.size():m.geometry.vertices.size();}
std::size_t triangles(const Object3DMesh &m)
{
    if(m.topology){if(!m.topology->layout)throw std::invalid_argument("Missing automatic fitting topology");return m.topology->layout->triangles().size()/3;}
    return m.geometry.triangles.size()/3;
}
struct Plan final
{
    std::vector<std::size_t> order,followers,rules;
};
Plan plan(const Object3DData &data,std::span<const SurfaceBinding> followers,std::span<const SurfaceFittingRule> rules,
          std::span<const std::optional<SurfaceBindingBasis>> bases)
{
    if(data.meshes.size()>4096||rules.size()>64)throw std::length_error("Automatic fitting graph limit");
    validateMeshSurfaceBindingsWithBases(data,followers,bases);const auto n=data.meshes.size();Plan result;result.followers.assign(n,Missing);result.rules.assign(n,Missing);
    std::vector<std::set<std::size_t>> edges(n);std::vector<std::size_t> degree(n);
    const auto edge=[&](std::size_t a,std::size_t b){if(edges[a].insert(b).second)++degree[b];};
    for(std::size_t i=0;i<followers.size();++i){const auto &b=followers[i];result.followers[b.targetMesh]=i;edge(b.sourceMesh,b.targetMesh);}
    std::set<std::size_t> targets;
    for(std::size_t i=0;i<rules.size();++i)
    {
        const auto &r=rules[i];if(r.targetMesh>=n||!targets.insert(r.targetMesh).second||r.obstacleMeshes.empty()||r.obstacleMeshes.size()>64)throw std::invalid_argument("Invalid or duplicate automatic fitting target");
        const auto options=r.fitting.options();validateSurfaceFitOptions(options,points(data.meshes[r.targetMesh]),triangles(data.meshes[r.targetMesh]));
        if(options.allowUnresolved)throw std::invalid_argument("Automatic fitting cannot publish unresolved previews");
        const auto &c=r.collider;
        if(!std::isfinite(c.minimumRelativeVolume)||c.minimumRelativeVolume<=0||c.minimumRelativeVolume>=1||!c.maximumPoints||!c.maximumTriangles||!c.maximumComponents||!c.maximumPreprocessVisits)throw std::invalid_argument("Invalid automatic fitting collider options");
        std::set<std::size_t> obstacles;std::size_t sourcePoints=0,sourceTriangles=0;
        for(auto source:r.obstacleMeshes)
        {
            if(source>=n||source==r.targetMesh||!obstacles.insert(source).second)throw std::invalid_argument("Invalid automatic fitting obstacle");
            charge(points(data.meshes[source]),c.maximumPoints,sourcePoints);charge(triangles(data.meshes[source]),c.maximumTriangles,sourceTriangles);
            if(r.enabled)edge(source,r.targetMesh);
        }
        if(r.enabled)result.rules[r.targetMesh]=i;
    }
    std::priority_queue<std::size_t,std::vector<std::size_t>,std::greater<>> ready;
    for(std::size_t i=0;i<n;++i)if(!degree[i])ready.push(i);
    while(!ready.empty()){auto i=ready.top();ready.pop();result.order.push_back(i);for(auto target:edges[i])if(--degree[target]==0)ready.push(target);}
    if(result.order.size()!=n)throw std::invalid_argument("Cyclic automatic fitting/follower graph");return result;
}
std::vector<std::uint32_t> alternateTriangles(const Object3DMesh &mesh)
{
    if(!mesh.topology)return {};const auto &t=*mesh.topology;const auto &d=t.layout->definition();
    const auto cached=t.layout->triangles(),polygons=t.layout->trianglePolygons();std::vector<std::vector<std::uint32_t>> original(d.polygons.size());
    for(std::size_t i=0;i<cached.size();++i)original.at(polygons[i/3]).push_back(d.layouts.at(d.renderToLayout.at(cached[i])).position);
    std::set<std::uint32_t> hidden(d.hiddenPolygons.begin(),d.hiddenPolygons.end());std::vector<std::uint32_t> result;bool changed=false;
    for(std::size_t f=0;f<d.polygons.size();++f)
    {
        if(hidden.contains(static_cast<std::uint32_t>(f)))continue;const auto &face=d.polygons[f];bool alternate=false;std::array<std::uint32_t,4> p{};
        if(face.size()==4)
        {
            for(unsigned k=0;k<4;++k)p[k]=d.layouts.at(face[k]).position;
            const auto a=sub(t.points[p[1]].position,t.points[p[0]].position),b=sub(t.points[p[2]].position,t.points[p[0]].position),c=sub(t.points[p[3]].position,t.points[p[0]].position);
            const auto normal=cross(a,b);const auto magnitude=length(normal),scale=std::max({length(a),length(b),length(c)});
            alternate=magnitude>0&&std::abs(dot(c,normal))/magnitude>scale*1e-12;
        }
        if(!alternate){result.insert(result.end(),original[f].begin(),original[f].end());continue;}
        changed=true;bool diagonal02=false;
        for(std::size_t k=0;k<original[f].size();k+=3){bool a=false,c=false;for(unsigned j=0;j<3;++j){a|=original[f][k+j]==p[0];c|=original[f][k+j]==p[2];}diagonal02|=a&&c;}
        if(diagonal02)result.insert(result.end(),{p[0],p[1],p[3],p[1],p[2],p[3]});else result.insert(result.end(),{p[0],p[1],p[2],p[0],p[2],p[3]});
    }
    return changed?std::move(result):std::vector<std::uint32_t>{};
}
void follow(Object3DData &data,const Object3DData &reference,const SurfaceBinding &binding,const SurfaceBindingBasis *basis)
{
    const auto now=basis?evaluateMeshSurfaceBindingWithBasis(data,binding,*basis):evaluateMeshSurfaceBinding(data,binding);
    const auto before=basis?evaluateMeshSurfaceBindingWithBasis(reference,binding,*basis):evaluateMeshSurfaceBinding(reference,binding);
    auto &target=data.meshes[binding.targetMesh];bool changed=false;
    for(std::size_t i=0;i<binding.points.size();++i)
    {
        const auto delta=sub(now[i],before[i]);if(!finite(delta))throw std::invalid_argument("Automatic follower displacement overflow");
        auto &position=target.topology?target.topology->points[binding.points[i].point].position:target.geometry.vertices[binding.points[i].point].position;
        const auto next=add(position,delta);if(!finite(next))throw std::invalid_argument("Automatic follower position overflow");position=next;changed|=delta!=Vector3{};
    }
    if(changed)object3d_detail::refreshEditedMesh(target,true);
}
}
SurfaceFitSettings::SurfaceFitSettings(const SurfaceFitOptions &options):parameters_(options)
{
    parameters_.mobility={};if(options.mobility.size()>options.maximumPoints)throw std::length_error("Owned surface mobility budget");
    mobility_.assign(options.mobility.begin(),options.mobility.end());
}
SurfaceFitOptions SurfaceFitSettings::options() const &{auto result=parameters_;result.mobility=mobility_;return result;}
bool SurfaceFitSettings::operator==(const SurfaceFitSettings &other) const{return parameters(parameters_)==parameters(other.parameters_)&&mobility_==other.mobility_;}
bool SurfaceFittingRule::operator==(const SurfaceFittingRule &other) const
{return targetMesh==other.targetMesh&&obstacleMeshes==other.obstacleMeshes&&fitting==other.fitting&&colliderParameters(collider)==colliderParameters(other.collider)&&alternateQuadDiagonals==other.alternateQuadDiagonals&&enabled==other.enabled;}
void validateObject3DSurfaceFitting(const Object3DData &data,std::span<const SurfaceBinding> followers,std::span<const SurfaceFittingRule> rules)
{(void)plan(data,followers,rules,{});}
void validateObject3DSurfaceFittingWithBases(const Object3DData &data,std::span<const SurfaceBinding> followers,
    std::span<const SurfaceFittingRule> rules,std::span<const std::optional<SurfaceBindingBasis>> bases)
{(void)plan(data,followers,rules,bases);}
std::vector<SurfaceFittingEvaluation> applyObject3DSurfaceFitting(Object3DData &data,std::span<const SurfaceBinding> followers,std::span<const SurfaceFittingRule> rules)
{return applyObject3DSurfaceFittingWithBases(data,followers,rules,{});}
std::vector<SurfaceFittingEvaluation> applyObject3DSurfaceFittingWithBases(Object3DData &data,std::span<const SurfaceBinding> followers,
    std::span<const SurfaceFittingRule> rules,std::span<const std::optional<SurfaceBindingBasis>> bases)
{
    if(rules.empty())return {};const auto execution=plan(data,followers,rules,bases);
    if(std::none_of(rules.begin(),rules.end(),[](const auto &r){return r.enabled;}))return {};
    auto next=data;const auto reference=data;std::vector<SurfaceFittingEvaluation> result;
    for(auto mesh:execution.order)
    {
        if(execution.followers[mesh]!=Missing)
        {
            const auto index=execution.followers[mesh];
            const auto *basis=bases.empty()||!bases[index]?nullptr:&*bases[index];
            follow(next,reference,followers[index],basis);
        }
        if(execution.rules[mesh]==Missing)continue;const auto &rule=rules[execution.rules[mesh]];
        std::vector<MeshCollider> snapshots;snapshots.reserve(rule.obstacleMeshes.size()*2);
        std::size_t pointBudget=0,triangleBudget=0,componentBudget=0,preprocessBudget=0;
        const auto colliderOptions=[&]
        {
            auto options=rule.collider;
            if(componentBudget>=options.maximumComponents||preprocessBudget>=options.maximumPreprocessVisits)throw std::length_error("Automatic fitting aggregate preprocessing budget");
            options.maximumComponents-=componentBudget;options.maximumPreprocessVisits-=preprocessBudget;return options;
        };
        const auto account=[&]
        {
            const auto &stats=snapshots.back().statistics();charge(stats.components,rule.collider.maximumComponents,componentBudget);charge(stats.preprocessVisits,rule.collider.maximumPreprocessVisits,preprocessBudget);
        };
        for(auto source:rule.obstacleMeshes)
        {
            const auto &obstacle=next.meshes[source];charge(points(obstacle),rule.collider.maximumPoints,pointBudget);charge(triangles(obstacle),rule.collider.maximumTriangles,triangleBudget);
            snapshots.push_back(MeshCollider::fromMesh(obstacle,colliderOptions()));account();
            if(rule.alternateQuadDiagonals)
            {
                const auto alternate=alternateTriangles(obstacle);
                if(!alternate.empty())
                {
                    charge(points(obstacle),rule.collider.maximumPoints,pointBudget);charge(alternate.size()/3,rule.collider.maximumTriangles,triangleBudget);
                    std::vector<Vector3> vertices;for(const auto &p:obstacle.topology->points)vertices.push_back(p.position);snapshots.emplace_back(vertices,alternate,colliderOptions());account();
                }
            }
        }
        std::vector<const SurfaceCollider*> obstacles;for(const auto &s:snapshots)obstacles.push_back(&s);
        result.push_back({mesh,fitMeshSurface(next.meshes[mesh],obstacles,rule.fitting.options())});
    }
    static_assert(std::is_nothrow_move_assignable_v<Object3DData>);data=std::move(next);return result;
}
} // namespace iiActorObject
