#include "MorphSymmetry.h"
#include "MeshEditGraph.h"
#include "MeshGeometryCore.h"
#include "CharacterMath.h"
#include <map>
#include <set>
#include <type_traits>

namespace iiActorObject
{
using namespace character_detail;
namespace
{
struct Budget final
{
    std::size_t used=0,limit;
    void visit(std::size_t count=1){if(count>limit-used)throw std::length_error("Morph mirror visit budget");used+=count;}
};
struct Dense final
{
    std::vector<Vector3> positions,normals;
};
Vector3 blendVector(Vector3 a,Vector3 b,double t)
{return {std::lerp(a.x,b.x,t),std::lerp(a.y,b.y,t),std::lerp(a.z,b.z,t)};}
void nameValid(std::string_view name)
{if(name.empty()||name.size()>1024||name.find('\0')!=name.npos)throw std::invalid_argument("Invalid mirror morph name");}
std::vector<std::uint32_t> canonicalCycle(std::vector<std::uint32_t> points,Budget &budget)
{
    if(points.empty())return points;std::size_t first=0;
    for(std::size_t i=1;i<points.size();++i)
    {
        budget.visit();if(points[i]>points[first])continue;if(points[i]<points[first]){first=i;continue;}
        for(std::size_t j=1;j<points.size();++j){budget.visit();const auto a=points[(i+j)%points.size()],b=points[(first+j)%points.size()];if(a==b)continue;if(a<b)first=i;break;}
    }
    std::rotate(points.begin(),points.begin()+first,points.end());return points;
}
using NormalSources=std::vector<std::vector<std::uint32_t>>;
NormalSources normalSources(const MeshTopology &t,const MeshSymmetryMap &mapping,Budget &budget)
{
    const auto &d=t.layout->definition();budget.visit(d.layouts.size());NormalSources sources(d.layouts.size()),pointLayouts(t.points.size());
    for(std::size_t i=0;i<d.layouts.size();++i)pointLayouts.at(d.layouts[i].position).push_back(static_cast<std::uint32_t>(i));
    std::map<std::vector<std::uint32_t>,std::vector<std::size_t>> faces;
    for(std::size_t f=0;f<d.polygons.size();++f)
    {std::vector<std::uint32_t> points;for(auto layout:d.polygons[f]){budget.visit();points.push_back(d.layouts.at(layout).position);}faces[canonicalCycle(std::move(points),budget)].push_back(f);}
    for(const auto &polygon:d.polygons)
    {
        std::vector<std::uint32_t> reflected;bool complete=true;
        for(auto layout:polygon){budget.visit();const auto point=mapping.mirrors()[d.layouts.at(layout).position];if(point==NoTopologyIndex)complete=false;reflected.push_back(point);}
        if(!complete)continue;std::reverse(reflected.begin(),reflected.end());const auto found=faces.find(canonicalCycle(std::move(reflected),budget));if(found==faces.end())continue;
        for(auto face:found->second)for(auto source:polygon)
        {
            const auto point=mapping.mirrors()[d.layouts[source].position];
            for(auto target:d.polygons[face]){budget.visit();if(d.layouts.at(target).position==point)sources[target].push_back(source);}
        }
    }
    for(std::size_t i=0;i<sources.size();++i)
    {
        if(sources[i].empty())
        {
            const auto point=mapping.mirrors()[d.layouts[i].position];
            if(point!=NoTopologyIndex){budget.visit(pointLayouts[point].size());sources[i]=pointLayouts[point];}
        }
        auto &row=sources[i];std::sort(row.begin(),row.end());row.erase(std::unique(row.begin(),row.end()),row.end());
    }
    return sources;
}
Dense dense(const MeshTopology &t,const TopologyMorph &m,bool normals)
{
    Dense result;result.positions.resize(t.points.size());std::set<std::uint32_t> seen;
    for(const auto &delta:m.deltas){if(!finite(delta.position)||!seen.insert(delta.point).second)throw std::invalid_argument("Invalid/duplicate source morph position");result.positions.at(delta.point)=delta.position;}
    if(normals)
    {
        result.normals.resize(t.layout->definition().layouts.size());seen.clear();
        const auto values=m.normalDeltas?*m.normalDeltas:topologyMorphNormalDeltas(t,m);
        for(const auto &delta:values){if(!finite(delta.normal)||!seen.insert(delta.layout).second)throw std::invalid_argument("Invalid/duplicate source morph normal");result.normals.at(delta.layout)=delta.normal;}
    }
    else if(m.normalDeltas)
    {
        seen.clear();for(const auto &delta:*m.normalDeltas)if(delta.layout>=t.layout->definition().layouts.size()||!finite(delta.normal)||!seen.insert(delta.layout).second)throw std::invalid_argument("Invalid source morph normal");
    }
    return result;
}
Dense dense(const SkeletalMeshLod &lod,const MorphTarget &m)
{
    Dense result;result.positions.resize(lod.vertices.size());result.normals.resize(lod.vertices.size());std::set<std::size_t> seen;
    for(const auto &delta:m.deltas)
    {if(!finite(delta.position)||!finite(delta.normal)||!seen.insert(delta.vertex).second)throw std::invalid_argument("Invalid/duplicate render morph delta");result.positions.at(delta.vertex)=delta.position;result.normals.at(delta.vertex)=delta.normal;}
    return result;
}
template<class Morph>std::size_t sourceIndex(const std::vector<Morph> &morphs,const MorphMirrorRequest &request)
{
    std::size_t found=morphs.size();
    for(std::size_t i=0;i<morphs.size();++i)
    {
        bool matches=morphs[i].name==request.source;
        if constexpr(std::is_same_v<Morph,TopologyMorph>)
        {if(request.sourceTarget!=NoTopologyIndex)matches=morphs[i].sourceTarget==request.sourceTarget&&(request.source.empty()||morphs[i].name==request.source);}
        else if(request.sourceTarget!=NoTopologyIndex)throw std::invalid_argument("Render morphs have no authored target ID");
        if(matches){if(found!=morphs.size())throw std::invalid_argument("Ambiguous source morph; specify its target ID");found=i;}
    }
    if(found==morphs.size())throw std::out_of_range("Unknown source mirror morph");return found;
}
template<class Morph>std::size_t targetIndex(const std::vector<Morph> &morphs,std::string_view name)
{
    std::size_t found=morphs.size();for(std::size_t i=0;i<morphs.size();++i)if(morphs[i].name==name){if(found!=morphs.size())throw std::invalid_argument("Ambiguous target mirror morph name");found=i;}return found;
}
bool same(const TopologyMorph &a,const TopologyMorph &b)
{
    if(a.name!=b.name||a.sourceTarget!=b.sourceTarget||a.deltas.size()!=b.deltas.size()||a.normalDeltas.has_value()!=b.normalDeltas.has_value())return false;
    for(std::size_t i=0;i<a.deltas.size();++i)if(a.deltas[i].point!=b.deltas[i].point||a.deltas[i].position!=b.deltas[i].position)return false;
    return !a.normalDeltas||*a.normalDeltas==*b.normalDeltas;
}
bool same(const MorphTarget &a,const MorphTarget &b)
{
    if(a.name!=b.name||a.deltas.size()!=b.deltas.size())return false;
    for(std::size_t i=0;i<a.deltas.size();++i)if(a.deltas[i].vertex!=b.deltas[i].vertex||a.deltas[i].position!=b.deltas[i].position||a.deltas[i].normal!=b.deltas[i].normal)return false;return true;
}
void addDelta(MorphMirrorResult &result,bool normal,const MorphMirrorOptions &o)
{
    if(result.positionDeltas+result.normalDeltas>=o.maximumMorphDeltas)throw std::length_error("Mirrored morph delta budget");
    if(normal)++result.normalDeltas;else ++result.positionDeltas;
}
std::vector<Vector3> reflectedPositions(const Dense &source,const Dense &old,const MeshSymmetryMap &mapping,const MorphMirrorOptions &o,MorphMirrorResult &report)
{
    auto result=old.positions;
    for(std::size_t i=0;i<result.size();++i)
    {
        const auto j=mapping.mirrors()[i];if(j==NoTopologyIndex){if(length(source.positions[i])>o.epsilon)++report.unmappedSourceDeltas;continue;}
        const auto value=blendVector(old.positions[i],mapping.reflectVector(source.positions[j]),o.mask.empty()?1:o.mask[i]);if(!finite(value))throw std::invalid_argument("Unrepresentable mirrored morph");result[i]=value;
    }
    return result;
}
TopologyMorph authoredMirror(const MeshTopology &t,const TopologyMorph &source,const TopologyMorph *old,const MorphMirrorRequest &request,const MeshSymmetryMap &mapping,const MorphMirrorOptions &o,MorphMirrorResult &report,std::optional<NormalSources> &sources,Budget &budget)
{
    const bool normals=o.preserveExplicitNormals&&(source.normalDeltas.has_value()||(old&&old->normalDeltas.has_value()));
    const auto a=dense(t,source,normals);auto b=old?dense(t,*old,normals):Dense{std::vector<Vector3>(t.points.size()),normals?std::vector<Vector3>(t.layout->definition().layouts.size()):std::vector<Vector3>{}};
    const auto points=reflectedPositions(a,b,mapping,o,report);TopologyMorph result;result.name=request.target;result.sourceTarget=old?old->sourceTarget:NoTopologyIndex;
    for(std::size_t i=0;i<points.size();++i)if(length(points[i])>o.epsilon){addDelta(report,false,o);result.deltas.push_back({static_cast<std::uint32_t>(i),points[i]});}
    if(normals)
    {
        if(!sources)sources=normalSources(t,mapping,budget);result.normalDeltas.emplace();const auto &d=t.layout->definition();std::vector<bool> covered(d.layouts.size(),false);
        for(std::size_t i=0;i<d.layouts.size();++i)
        {
            budget.visit();const auto p=d.layouts[i].position;auto value=b.normals[i];
            if(mapping.mirrors()[p]!=NoTopologyIndex&&(o.mask.empty()||o.mask[p]>0))
            {
                Vector3 reflected;
                if(!(*sources)[i].empty())reflected=mapping.reflectVector(a.normals[(*sources)[i][0]]);
                for(auto id:(*sources)[i]){budget.visit();covered[id]=true;if(length(sub(mapping.reflectVector(a.normals[id]),reflected))>o.epsilon)throw std::invalid_argument("Ambiguous mirrored layout normals; choose explicit recomputation");}
                value=blendVector(value,reflected,o.mask.empty()?1:o.mask[p]);
            }
            if(!finite(value))throw std::invalid_argument("Unrepresentable mirrored morph normal");
            if(length(value)>o.epsilon){addDelta(report,true,o);result.normalDeltas->push_back({static_cast<std::uint32_t>(i),value});}
        }
        for(std::size_t i=0;i<a.normals.size();++i)if(length(a.normals[i])>o.epsilon&&!covered[i])
        {const auto target=mapping.mirrors()[d.layouts[i].position];if(target==NoTopologyIndex)++report.unmappedSourceDeltas;else if(o.mask.empty()||o.mask[target]>0)throw std::invalid_argument("Missing mirrored normal correspondence");}
    }
    return result;
}
MorphTarget renderMirror(const SkeletalMeshLod &lod,const MorphTarget &source,const MorphTarget *old,const MorphMirrorRequest &request,const MeshSymmetryMap &mapping,const MorphMirrorOptions &o,MorphMirrorResult &report)
{
    const auto a=dense(lod,source),b=old?dense(lod,*old):Dense{std::vector<Vector3>(lod.vertices.size()),std::vector<Vector3>(lod.vertices.size())};
    const auto positions=reflectedPositions(a,b,mapping,o,report);auto normals=b.normals;
    for(std::size_t i=0;i<normals.size();++i)
    {
        const auto j=mapping.mirrors()[i];if(j==NoTopologyIndex){if(length(a.normals[i])>o.epsilon)++report.unmappedSourceDeltas;continue;}
        normals[i]=blendVector(normals[i],mapping.reflectVector(a.normals[j]),o.mask.empty()?1:o.mask[i]);
    }
    if(!o.preserveExplicitNormals)
    {
        auto derived=lod;MorphTarget positionOnly;positionOnly.name=request.target;
        for(std::size_t i=0;i<positions.size();++i)if(length(positions[i])>o.epsilon)positionOnly.deltas.push_back({i,positions[i],{}});
        derived.morphTargets={std::move(positionOnly)};recomputeNormals(derived);normals.clear();
        for(std::size_t i=0;i<lod.vertices.size();++i)normals.push_back(sub(derived.vertices[i].normal,lod.vertices[i].normal));
        for(const auto &delta:derived.morphTargets[0].deltas)normals[delta.vertex]=add(normals[delta.vertex],delta.normal);
    }
    MorphTarget result;result.name=request.target;
    for(std::size_t i=0;i<positions.size();++i)
    {
        if(!finite(normals[i]))throw std::invalid_argument("Unrepresentable mirrored render normal");const bool p=length(positions[i])>o.epsilon,n=length(normals[i])>o.epsilon;
        if(p||n){if(p)addDelta(report,false,o);if(n)addDelta(report,true,o);result.deltas.push_back({i,p?positions[i]:Vector3{},n?normals[i]:Vector3{}});}
    }
    return result;
}
}
std::vector<double> makeMorphSymmetryMask(const MeshSymmetryMap &mapping,MeshSymmetryDirection direction,double strength)
{
    weight(strength);if(direction<MeshSymmetryDirection::NegativeToPositive||direction>MeshSymmetryDirection::Average)throw std::invalid_argument("Invalid morph symmetry direction");
    if(mapping.mirrors().empty())throw std::logic_error("Empty symmetry correspondence");std::vector<double> result(mapping.mirrors().size());
    for(std::size_t i=0;i<result.size();++i)
    {
        if(mapping.mirrors()[i]==NoTopologyIndex)continue;const auto side=mapping.sides()[i];
        if(side==0||direction==MeshSymmetryDirection::Average)result[i]=strength*.5;
        else if((direction==MeshSymmetryDirection::NegativeToPositive&&side>0)||(direction==MeshSymmetryDirection::PositiveToNegative&&side<0))result[i]=strength;
    }
    return result;
}
MorphMirrorResult mirrorMorphs(Object3DMesh &mesh,const MeshSymmetryMap &mapping,std::span<const MorphMirrorRequest> requests,const MorphMirrorOptions &o)
{
    mapping.validateFor(mesh);if(!std::isfinite(o.epsilon)||o.epsilon<0)throw std::invalid_argument("Invalid morph mirror tolerance");
    if(!o.maximumPointVisits||!o.maximumMorphDeltas||requests.size()>4096||requests.size()>o.maximumPointVisits/mapping.mirrors().size())throw std::length_error("Morph mirror request budget");
    if(!o.mask.empty())object3d_detail::validatePointWeights(o.mask,mapping.mirrors().size());
    const auto layouts=mesh.topology?mesh.topology->layout->definition().layouts.size():mesh.geometry.vertices.size();
    if(!o.maximumLayouts||layouts>o.maximumPointVisits||layouts>o.maximumLayouts)throw std::length_error("Morph mirror layout budget");
    std::set<std::string> targets;for(const auto &r:requests){nameValid(r.target);if(!r.source.empty())nameValid(r.source);else if(r.sourceTarget==NoTopologyIndex)throw std::invalid_argument("Empty source mirror morph");if(!targets.insert(r.target).second)throw std::invalid_argument("Duplicate mirror destination");}
    bool inactive=true;for(std::size_t i=0;i<mapping.mirrors().size();++i)if(mapping.mirrors()[i]!=NoTopologyIndex&&(o.mask.empty()||o.mask[i]>0)){inactive=false;break;}
    MorphMirrorResult report;if(requests.empty())return report;Budget budget{requests.size()*mapping.mirrors().size(),o.maximumPointVisits};auto next=mesh;std::optional<NormalSources> sources;
    if(mesh.topology)
    {
        const auto &original=*mesh.topology;
        for(const auto &r:requests)
        {
            const auto source=sourceIndex(original.morphTargets,r),target=targetIndex(original.morphTargets,r.target);const auto *old=target==original.morphTargets.size()?nullptr:&original.morphTargets[target];
            if(old&&!o.replaceExisting)throw std::invalid_argument("Mirror destination already exists");
            if(old&&inactive)continue;
            auto morph=authoredMirror(original,original.morphTargets[source],old,r,mapping,o,report,sources,budget);
            if(!old){next.topology->morphTargets.push_back(std::move(morph));++report.created;}
            else if(!same(*old,morph)){next.topology->morphTargets[target]=std::move(morph);++report.replaced;}
        }
    }
    else
    {
        const auto &original=mesh.geometry;
        for(const auto &r:requests)
        {
            const auto source=sourceIndex(original.morphTargets,r),target=targetIndex(original.morphTargets,r.target);const auto *old=target==original.morphTargets.size()?nullptr:&original.morphTargets[target];
            if(old&&!o.replaceExisting)throw std::invalid_argument("Mirror destination already exists");if(old&&inactive)continue;auto morph=renderMirror(original,original.morphTargets[source],old,r,mapping,o,report);
            if(!old){next.geometry.morphTargets.push_back(std::move(morph));++report.created;}else if(!same(*old,morph)){next.geometry.morphTargets[target]=std::move(morph);++report.replaced;}
        }
    }
    if(report.changed()){object3d_detail::refreshEditedMesh(next,false);static_assert(std::is_nothrow_move_assignable_v<Object3DMesh>);mesh=std::move(next);}return report;
}
} // namespace iiActorObject
