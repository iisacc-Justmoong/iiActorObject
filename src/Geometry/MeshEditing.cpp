#include "MeshEditGraph.h"
#include "CharacterMath.h"
#include "MeshGeometryCore.h"
#include <numeric>
#include <queue>
#include <set>

namespace iiActorObject
{
using namespace character_detail;
namespace object3d_detail
{
MeshEditGraph::MeshEditGraph(const Object3DMesh &mesh)
{
    if(mesh.topology)
    {
        const auto &t=*mesh.topology;if(!t.layout)throw std::invalid_argument("Missing edit topology layout");const auto &d=t.layout->definition();
        if(t.points.size()!=d.positionCount)throw std::invalid_argument("Edit topology point dimensions");
        for(const auto &p:t.points)positions.push_back(p.position);
        for(auto l:d.renderToLayout)renderToPoint.push_back(d.layouts.at(l).position);
        triangles.assign(t.layout->triangles().begin(),t.layout->triangles().end());triangleFaces.assign(t.layout->trianglePolygons().begin(),t.layout->trianglePolygons().end());
        std::set<std::uint32_t> hidden(d.hiddenPolygons.begin(),d.hiddenPolygons.end());
        for(std::size_t f=0;f<d.polygons.size();++f)
        {
            auto &p=polygons.emplace_back();if(hidden.contains(static_cast<std::uint32_t>(f)))continue;
            for(auto l:d.polygons[f])p.push_back(d.layouts.at(l).position);
        }
    }
    else
    {
        for(const auto &v:mesh.geometry.vertices)positions.push_back(v.position);triangles=mesh.geometry.triangles;
        if(triangles.size()%3)throw std::invalid_argument("Edit triangle count");renderToPoint.resize(positions.size());std::iota(renderToPoint.begin(),renderToPoint.end(),0u);
        for(std::size_t i=0;i<triangles.size();i+=3){triangleFaces.push_back(static_cast<std::uint32_t>(polygons.size()));polygons.push_back({triangles[i],triangles[i+1],triangles[i+2]});}
    }
    if(positions.empty()||positions.size()>=NoTopologyIndex)throw std::invalid_argument("Edit point count");
    for(auto p:positions)if(!finite(p))throw std::invalid_argument("Nonfinite edit point");
    if(triangleFaces.size()!=triangles.size()/3)throw std::invalid_argument("Edit triangle source mapping");
    visible.resize(positions.size(),false);neighbours.resize(positions.size());
    for(const auto &polygon:polygons)for(std::size_t i=0;i<polygon.size();++i)
    {
        const auto a=polygon[i],b=polygon[(i+1)%polygon.size()];if(a>=positions.size()||b>=positions.size())throw std::out_of_range("Edit polygon point");
        visible[a]=true;if(a!=b){neighbours[a].push_back(b);neighbours[b].push_back(a);}
    }
    for(auto &row:neighbours){std::sort(row.begin(),row.end());row.erase(std::unique(row.begin(),row.end()),row.end());}
}
void MeshEditGraph::validate(const MeshBrushSettings &s,std::size_t samples) const
{
    if(samples>4096||!s.maximumPointVisits||samples>s.maximumPointVisits/positions.size())throw std::length_error("Brush stroke visit budget");
    if(!s.mask.empty())validatePointWeights(s.mask,positions.size());
    if(s.falloff<MeshBrushFalloff::Constant||s.falloff>MeshBrushFalloff::Sharp||s.distance<MeshBrushDistance::Euclidean||s.distance>MeshBrushDistance::Surface)throw std::invalid_argument("Unknown brush kernel");
}
std::vector<double> MeshEditGraph::footprint(MeshBrushSample sample,const MeshBrushSettings &s)
{
    if(!finite(sample.center)||!std::isfinite(sample.radius)||sample.radius<=0||!std::isfinite(sample.strength)||sample.strength<0||sample.strength>1)throw std::invalid_argument("Invalid brush sample");
    validate(s,1);std::vector<double> distance(positions.size(),std::numeric_limits<double>::infinity()),result(positions.size(),0);
    if(sample.strength==0)return result;
    if(s.distance==MeshBrushDistance::Euclidean)
    {for(std::size_t p=0;p<positions.size();++p)distance[p]=length(sub(positions[p],sample.center));}
    else
    {
        if(!surface){std::vector<Vector3> points;for(auto p:renderToPoint)points.push_back(positions.at(p));surface=std::make_unique<MeshSurface>(points,triangles);}
        const auto hit=surface->closest(sample.center,sample.radius);if(!hit.valid())return result;
        using QueueItem=std::pair<double,std::uint32_t>;std::priority_queue<QueueItem,std::vector<QueueItem>,std::greater<>> queue;
        for(auto p:polygons.at(triangleFaces.at(hit.triangle)))
        {const auto d=length(sub(positions[p],sample.center));if(d<distance[p]){distance[p]=d;queue.push({d,p});}}
        while(!queue.empty())
        {
            const auto [d,p]=queue.top();queue.pop();if(d>sample.radius)break;if(d!=distance[p])continue;
            for(auto n:neighbours[p]){const auto next=d+length(sub(positions[n],positions[p]));if(next<distance[n]&&next<=sample.radius){distance[n]=next;queue.push({next,n});}}
        }
    }
    for(std::size_t p=0;p<positions.size();++p)
    {
        if((!s.includeUnusedPoints&&!visible[p])||distance[p]>sample.radius)continue;
        auto value=std::clamp(1-distance[p]/sample.radius,0.0,1.0);
        switch(s.falloff){case MeshBrushFalloff::Constant:value=1;break;case MeshBrushFalloff::Linear:break;case MeshBrushFalloff::Smooth:value=value*value*(3-2*value);break;case MeshBrushFalloff::Sharp:value*=value;break;}
        result[p]=value*sample.strength*(s.mask.empty()?1:s.mask[p]);
    }
    return result;
}
void refreshEditedMesh(Object3DMesh &mesh,bool positionsChanged)
{
    if(mesh.topology)
    {
        if(positionsChanged)recomputeTopologyNormals(*mesh.topology);auto r=renderMeshTopology(*mesh.topology);mesh.geometry=std::move(r.geometry);mesh.textureCoordinates=std::move(r.textureCoordinates);mesh.triangleMaterials=std::move(r.triangleMaterials);
    }
    else if(positionsChanged){auto morphs=std::move(mesh.geometry.morphTargets);mesh.geometry.morphTargets.clear();recomputeNormals(mesh.geometry);mesh.geometry.morphTargets=std::move(morphs);}
}
}
std::vector<double> makeMeshPointMask(const Object3DMesh &mesh,std::span<const std::uint32_t> points,double value)
{
    object3d_detail::validatePointWeights(std::span(&value,1));object3d_detail::MeshEditGraph graph(mesh);std::vector<double> result(graph.positions.size(),0);std::set<std::uint32_t> seen;
    for(auto p:points){if(!seen.insert(p).second)throw std::invalid_argument("Duplicate mask point");result.at(p)=value;}return result;
}
std::vector<double> makeMeshBrushMask(const Object3DMesh &mesh,MeshBrushSample sample,const MeshBrushSettings &settings)
{object3d_detail::MeshEditGraph graph(mesh);return graph.footprint(sample,settings);}
std::vector<double> growMeshPointMask(const Object3DMesh &mesh,std::span<const double> mask,unsigned steps,double decay)
{
    object3d_detail::MeshEditGraph graph(mesh);object3d_detail::validatePointWeights(mask,graph.positions.size());object3d_detail::validatePointWeights(std::span(&decay,1));
    if(steps>4096)throw std::length_error("Mask growth limit");std::vector<double> result(mask.begin(),mask.end());
    for(unsigned i=0;i<steps;++i){auto next=result;for(std::size_t p=0;p<result.size();++p)for(auto n:graph.neighbours[p])next[n]=std::max(next[n],result[p]*decay);result=std::move(next);}return result;
}
std::vector<double> smoothMeshPointMask(const Object3DMesh &mesh,std::span<const double> mask,double strength,unsigned iterations)
{
    object3d_detail::MeshEditGraph graph(mesh);object3d_detail::validatePointWeights(mask,graph.positions.size());object3d_detail::validatePointWeights(std::span(&strength,1));
    if(iterations>4096)throw std::length_error("Mask smoothing limit");std::vector<double> result(mask.begin(),mask.end());
    for(unsigned i=0;i<iterations;++i){auto next=result;for(std::size_t p=0;p<result.size();++p)if(!graph.neighbours[p].empty()){double total=0;for(auto n:graph.neighbours[p])total+=result[n];next[p]=std::lerp(result[p],total/graph.neighbours[p].size(),strength);}result=std::move(next);}return result;
}
std::vector<double> invertMeshPointMask(std::span<const double> mask)
{object3d_detail::validatePointWeights(mask);std::vector<double> result;for(auto v:mask)result.push_back(1-v);return result;}
std::vector<double> combineMeshPointMasks(std::span<const double> a,std::span<const double> b,MeshMaskCombine operation)
{
    if(operation<MeshMaskCombine::Union||operation>MeshMaskCombine::Multiply)throw std::invalid_argument("Unknown mask combination");
    object3d_detail::validatePointWeights(a);object3d_detail::validatePointWeights(b,a.size());std::vector<double> result(a.size());
    for(std::size_t i=0;i<a.size();++i)switch(operation){case MeshMaskCombine::Union:result[i]=std::max(a[i],b[i]);break;case MeshMaskCombine::Intersect:result[i]=std::min(a[i],b[i]);break;case MeshMaskCombine::Subtract:result[i]=std::max(0.0,a[i]-b[i]);break;case MeshMaskCombine::Multiply:result[i]=a[i]*b[i];break;default:throw std::invalid_argument("Unknown mask combination");}return result;
}
BrushStrokeResult MeshSculptor::stroke(Object3DData &data,std::size_t mesh,std::span<const SculptStrokeSample> samples,const MeshBrushSettings &settings,std::optional<BrushMirrorPlane> symmetry)
{
    if(samples.size()>4096)throw std::length_error("Sculpt stroke sample count");
    object3d_detail::MeshEditGraph graph(data.meshes.at(mesh));graph.validate(settings,samples.size());
    if(symmetry&&samples.size()>settings.maximumPointVisits/graph.positions.size()/2)throw std::length_error("Mirrored sculpt stroke visit budget");
    auto next=data;auto &part=next.meshes[mesh];std::vector<bool> touched(graph.positions.size(),false);Vector3 normal;
    if(symmetry){if(!std::isfinite(symmetry->offset))throw std::invalid_argument("Invalid brush mirror offset");normal=unit(symmetry->normal);}
    for(const auto &s:samples)
    {
        if(!finite(s.displacement))throw std::invalid_argument("Nonfinite sculpt displacement");auto a=graph.footprint(s.brush,settings);std::vector<double> b;Vector3 mirrored;
        if(symmetry){auto sample=s.brush;sample.center=sub(sample.center,mul(normal,2*(dot(sample.center,normal)-symmetry->offset)));b=graph.footprint(sample,settings);mirrored=sub(s.displacement,mul(normal,2*dot(s.displacement,normal)));}
        for(std::size_t p=0;p<a.size();++p)
        {
            if(a[p]==0&&(!symmetry||b[p]==0))continue;touched[p]=true;auto delta=mul(s.displacement,a[p]);
            if(symmetry){const auto total=a[p]+b[p];delta=mul(add(delta,mul(mirrored,b[p])),std::max(a[p],b[p])/total);}
            auto &position=part.topology?part.topology->points[p].position:part.geometry.vertices[p].position;position=add(position,delta);if(!finite(position))throw std::overflow_error("Sculpt stroke position overflow");
        }
    }
    BrushStrokeResult result;for(std::size_t p=0;p<touched.size();++p){if(touched[p])++result.touchedPoints;const auto v=part.topology?part.topology->points[p].position:part.geometry.vertices[p].position;if(std::abs(v.x-graph.positions[p].x)>=1e-12||std::abs(v.y-graph.positions[p].y)>=1e-12||std::abs(v.z-graph.positions[p].z)>=1e-12)++result.changedPoints;}
    if(result.changedPoints){object3d_detail::refreshEditedMesh(part,true);data=std::move(next);}return result;
}
} // namespace iiActorObject
