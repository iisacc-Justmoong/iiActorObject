#include "MeshCollider.h"
#include "MeshSurface.h"
#include "MeshEditGraph.h"
#include "CharacterMath.h"
#include <map>
#include <numbers>
#include <queue>
#include <tuple>

namespace iiActorObject
{
using namespace character_detail;
namespace
{
struct Work final
{
    std::size_t count=0,maximum;
    void visit(){if(count>=maximum)throw std::length_error("Collision preprocessing budget");++count;}
};
using EdgeKey=std::pair<std::uint32_t,std::uint32_t>;
EdgeKey edgeKey(std::uint32_t a,std::uint32_t b){return std::minmax(a,b);}
Vector3 divide(Vector3 p,double scale){return {p.x/scale,p.y/scale,p.z/scale};}
struct Edge final
{
    std::array<std::uint32_t,2> faces{NoTopologyIndex,NoTopologyIndex};
    std::array<unsigned,2> corners{};
    std::array<std::int8_t,2> directions{};
};
}
struct MeshCollider::Impl final
{
    MeshColliderStatistics stats;
    std::vector<Vector3> points,faceNormals,vertexNormals;
    std::vector<double> faceScales;
    std::vector<std::uint32_t> triangles,sourceTriangles;
    std::vector<std::array<Vector3,3>> edgeNormals;
    std::unique_ptr<MeshSurface> surface;
    Impl(std::span<const Vector3> input,std::span<const std::uint32_t> indices,const MeshColliderOptions &o)
    {
        if(!o.maximumPoints||!o.maximumTriangles||!o.maximumComponents||!o.maximumPreprocessVisits||input.size()>o.maximumPoints||indices.size()/3>o.maximumTriangles)throw std::length_error("Collision mesh input budget");
        if(!std::isfinite(o.minimumRelativeVolume)||o.minimumRelativeVolume<=0||o.minimumRelativeVolume>=1)throw std::invalid_argument("Invalid collision volume threshold");
        surface=std::make_unique<MeshSurface>(input,indices);points.assign(input.begin(),input.end());triangles.assign(indices.begin(),indices.end());sourceTriangles=triangles;
        const auto count=triangles.size()/3;stats.inputVertices=stats.collisionVertices=points.size();stats.triangles=count;Work work{0,o.maximumPreprocessVisits};
        std::map<EdgeKey,Edge> edges;std::vector<std::vector<std::uint32_t>> incident(points.size());
        std::vector<std::array<std::uint32_t,3>> neighbours(count,std::array<std::uint32_t,3>{NoTopologyIndex,NoTopologyIndex,NoTopologyIndex});std::vector<std::array<std::int8_t,3>> relations(count);
        for(std::size_t f=0;f<count;++f)for(unsigned k=0;k<3;++k)
        {
            work.visit();const auto a=triangles[f*3+k],b=triangles[f*3+(k+1)%3];auto &edge=edges[edgeKey(a,b)];const unsigned slot=edge.faces[0]==NoTopologyIndex?0:1;
            if(edge.faces[slot]!=NoTopologyIndex)throw std::invalid_argument("Nonmanifold collision edge");edge.faces[slot]=static_cast<std::uint32_t>(f);edge.corners[slot]=k;edge.directions[slot]=a<b?1:-1;incident[a].push_back(static_cast<std::uint32_t>(f));
        }
        for(const auto &[key,edge]:edges)
        {
            (void)key;work.visit();if(edge.faces[1]==NoTopologyIndex)throw std::invalid_argument("Collision boundary must be closed");
            for(unsigned side=0;side<2;++side){neighbours[edge.faces[side]][edge.corners[side]]=edge.faces[1-side];relations[edge.faces[side]][edge.corners[side]]=static_cast<std::int8_t>(-edge.directions[0]*edge.directions[1]);}
        }
        std::vector<std::int8_t> coherent(count);std::vector<std::uint32_t> component(count,NoTopologyIndex);std::vector<std::vector<std::uint32_t>> components;
        for(std::size_t seed=0;seed<count;++seed)if(!coherent[seed])
        {
            if(components.size()>=o.maximumComponents)throw std::length_error("Collision component budget");const auto id=static_cast<std::uint32_t>(components.size());components.emplace_back();
            std::queue<std::uint32_t> queue;queue.push(static_cast<std::uint32_t>(seed));coherent[seed]=1;component[seed]=id;
            while(!queue.empty())
            {
                const auto f=queue.front();queue.pop();components[id].push_back(f);
                for(unsigned k=0;k<3;++k){work.visit();const auto next=neighbours[f][k];const auto sign=static_cast<std::int8_t>(coherent[f]*relations[f][k]);if(coherent[next]&&coherent[next]!=sign)throw std::invalid_argument("Nonorientable collision boundary");if(!coherent[next]){coherent[next]=sign;component[next]=id;queue.push(next);}}
            }
        }
        // Each vertex link must be one cycle, not several edge-manifold sheets touching at a point.
        std::vector<std::uint32_t> stamps(count);
        for(std::size_t v=0;v<incident.size();++v)if(!incident[v].empty())
        {
            const auto stamp=static_cast<std::uint32_t>(v+1);std::vector<std::uint32_t> stack{incident[v][0]};std::size_t visited=0;
            while(!stack.empty())
            {
                work.visit();const auto f=stack.back();stack.pop_back();if(stamps[f]==stamp)continue;stamps[f]=stamp;++visited;
                unsigned k=0;while(k<3&&triangles[f*3+k]!=v)++k;if(k==3)throw std::invalid_argument("Invalid collision vertex fan");stack.push_back(neighbours[f][k]);stack.push_back(neighbours[f][(k+2)%3]);
            }
            if(visited!=incident[v].size())throw std::invalid_argument("Nonmanifold collision vertex fan");
        }
        std::vector<std::int8_t> outward(components.size());
        for(std::size_t i=0;i<components.size();++i)
        {
            const auto origin=points[triangles[components[i][0]*3]];Vector3 low=origin,high=origin;
            for(auto f:components[i])for(unsigned k=0;k<3;++k){work.visit();const auto p=points[triangles[f*3+k]];low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};}
            const auto extent=sub(high,low);const auto scale=std::max({extent.x,extent.y,extent.z});if(!(scale>0)||!std::isfinite(scale))throw std::invalid_argument("Unrepresentable collision extent");long double volume=0;
            for(auto f:components[i]){work.visit();const auto a=divide(sub(points[triangles[f*3]],origin),scale),b=divide(sub(points[triangles[f*3+1]],origin),scale),c=divide(sub(points[triangles[f*3+2]],origin),scale);volume+=static_cast<long double>(dot(a,cross(b,c)))*coherent[f];}
            if(std::abs(volume)<=o.minimumRelativeVolume)throw std::invalid_argument("Collision component has negligible enclosed volume");outward[i]=volume>0?1:-1;
        }
        std::vector<unsigned> depth(components.size());
        for(std::size_t i=0;i<components.size();++i)for(std::size_t j=0;j<components.size();++j)if(i!=j)
        {
            const auto p=points[triangles[components[i][0]*3]];long double angle=0;
            for(auto f:components[j])
            {
                work.visit();const auto a=unit(sub(points[triangles[f*3]],p)),b=unit(sub(points[triangles[f*3+1]],p)),c=unit(sub(points[triangles[f*3+2]],p));
                angle+=static_cast<long double>(2*std::atan2(dot(a,cross(b,c)),1+dot(a,b)+dot(a,c)+dot(b,c)))*coherent[f]*outward[j];
            }
            const auto winding=std::abs(angle)/(4*std::numbers::pi_v<long double>);
            if(!std::isfinite(winding)||winding>1.000001L||std::abs(winding-std::round(winding))>1e-6L)throw std::invalid_argument("Ambiguous intersecting/touching collision components");if(winding>.5L)++depth[i];
        }
        stats.components=components.size();faceNormals.resize(count);faceScales.resize(count);vertexNormals.resize(points.size());edgeNormals.resize(count);
        for(std::size_t f=0;f<count;++f)
        {
            work.visit();const auto a=points[triangles[f*3]],b=points[triangles[f*3+1]],c=points[triangles[f*3+2]];const auto u=sub(b,a),v=sub(c,a);const auto scale=std::max(length(u),length(v));
            faceScales[f]=scale;
            const int sign=coherent[f]*outward[component[f]]*(depth[component[f]]%2?-1:1);if(sign<0)++stats.reorientedTriangles;
            faceNormals[f]=mul(unit(cross(divide(u,scale),divide(v,scale))),sign);
            for(unsigned k=0;k<3;++k)
            {
                const auto point=triangles[f*3+k];const auto e0=unit(sub(points[triangles[f*3+(k+1)%3]],points[point])),e1=unit(sub(points[triangles[f*3+(k+2)%3]],points[point]));
                const auto weight=std::atan2(length(cross(e0,e1)),dot(e0,e1));vertexNormals[point]=add(vertexNormals[point],mul(faceNormals[f],weight));
            }
        }
        for(std::size_t v=0;v<vertexNormals.size();++v)if(!incident[v].empty())vertexNormals[v]=unit(vertexNormals[v]);
        for(std::size_t f=0;f<count;++f)for(unsigned k=0;k<3;++k)
        {
            work.visit();const auto &edge=edges.at(edgeKey(triangles[f*3+(k+1)%3],triangles[f*3+(k+2)%3]));edgeNormals[f][k]=unit(add(faceNormals[edge.faces[0]],faceNormals[edge.faces[1]]));
        }
        stats.preprocessVisits=work.count;
    }
    SurfaceCollisionSample sample(Vector3 point,SurfaceQueryBudget &budget) const
    {
        auto hit=surface->closest(point,std::numeric_limits<double>::infinity(),budget);if(!hit.valid()||!std::isfinite(hit.distance))throw std::invalid_argument("Unrepresentable collision sample");const auto f=hit.triangle;
        unsigned zeros=0,zero=0,vertex=0;for(unsigned k=0;k<3;++k){if(hit.barycentric[k]==0){++zeros;zero=k;}if(hit.barycentric[k]>hit.barycentric[vertex])vertex=k;}
        const auto normal=zeros>=2?vertexNormals[triangles[f*3+vertex]]:zeros==1?edgeNormals[f][zero]:faceNormals[f];const auto delta=sub(point,hit.position);const int sign=dot(delta,normal)<0?-1:1;
        const auto scale=std::max({faceScales[f],std::abs(point.x),std::abs(point.y),std::abs(point.z),std::abs(hit.position.x),std::abs(hit.position.y),std::abs(hit.position.z)});
        // Do not turn nearest-point roundoff on the boundary into a unit tangent.
        const auto gradient=hit.distance>scale*std::numeric_limits<double>::epsilon()*32?mul(delta,sign/hit.distance):normal;
        if(!finite(gradient))throw std::invalid_argument("Unrepresentable collision gradient");
        for(unsigned k=0;k<3;++k)hit.vertices[k]=sourceTriangles[f*3+k];return {sign*hit.distance,gradient,hit};
    }
};
MeshCollider::MeshCollider(std::span<const Vector3> points,std::span<const std::uint32_t> triangles,const MeshColliderOptions &options)
    :impl_(std::make_unique<Impl>(points,triangles,options)){}
MeshCollider MeshCollider::fromMesh(const Object3DMesh &mesh,const MeshColliderOptions &o)
{
    const auto count=mesh.topology?mesh.topology->points.size():mesh.geometry.vertices.size();if(count>o.maximumPoints)throw std::length_error("Collision source point budget");
    object3d_detail::MeshEditGraph graph(mesh);std::vector<std::uint32_t> triangles;triangles.reserve(graph.triangles.size());for(auto r:graph.triangles)triangles.push_back(graph.renderToPoint.at(r));
    if(mesh.topology||!o.weldRenderSeams)return MeshCollider(graph.positions,triangles,o);
    std::map<std::tuple<double,double,double>,std::uint32_t> unique;std::vector<Vector3> points;std::vector<std::uint32_t> mapping;
    for(auto p:graph.positions)
    {
        const auto key=std::tuple{p.x,p.y,p.z};auto found=unique.find(key);if(found==unique.end()){const auto index=static_cast<std::uint32_t>(points.size());points.push_back(p);found=unique.emplace(key,index).first;}mapping.push_back(found->second);
    }
    auto welded=triangles;for(auto &v:welded)v=mapping.at(v);MeshCollider result(points,welded,o);result.impl_->sourceTriangles=std::move(triangles);result.impl_->stats.inputVertices=graph.positions.size();return result;
}
MeshCollider::~MeshCollider()=default;
MeshCollider::MeshCollider(MeshCollider&&) noexcept=default;
MeshCollider &MeshCollider::operator=(MeshCollider&&) noexcept=default;
const MeshColliderStatistics &MeshCollider::statistics() const{if(!impl_)throw std::logic_error("Moved collision snapshot");return impl_->stats;}
SurfaceCollisionSample MeshCollider::sample(Vector3 p) const{SurfaceQueryBudget budget;return sample(p,budget);}
SurfaceCollisionSample MeshCollider::sample(Vector3 p,SurfaceQueryBudget &budget) const{if(!impl_)throw std::logic_error("Moved collision snapshot");return impl_->sample(p,budget);}
SurfaceCollisionContact MeshCollider::triangleContact(const std::array<Vector3,3> &triangle,double maximumDistance,SurfaceQueryBudget &budget) const
{
    if(!impl_)throw std::logic_error("Moved collision snapshot");const auto pair=impl_->surface->closestTriangle(triangle,maximumDistance,budget);if(!pair.valid())return {};
    return {pair.source.distance,pair.source.position,pair.targetPosition,pair.targetBarycentric,pair.source.triangle};
}
} // namespace iiActorObject
