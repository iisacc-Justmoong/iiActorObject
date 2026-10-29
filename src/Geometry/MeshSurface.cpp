#include "MeshSurface.h"
#include "CharacterMath.h"
#include <numeric>

namespace iiActorObject
{
namespace
{
using namespace character_detail;
double component(Vector3 v,unsigned axis){return axis==0?v.x:axis==1?v.y:v.z;}
Vector3 divide(Vector3 v,double scale){return {v.x/scale,v.y/scale,v.z/scale};}
double extent(Vector3 v){return std::max({std::abs(v.x),std::abs(v.y),std::abs(v.z)});}
void candidate(SurfaceHit &hit,Vector3 query,Vector3 a,Vector3 b,Vector3 c,std::array<double,3> bary)
{
    const auto point=add(add(mul(a,bary[0]),mul(b,bary[1])),mul(c,bary[2]));const auto distance=length(sub(point,query));
    if(!finite(point)||!std::isfinite(distance))throw std::overflow_error("Surface query distance overflow");
    if(distance<hit.distance){hit.position=point;hit.barycentric=bary;hit.distance=distance;}
}
double segment(Vector3 query,Vector3 a,Vector3 b)
{
    const auto edge=sub(b,a),delta=sub(query,a);const auto size=length(edge),scale=extent(delta);
    if(!finite(delta))throw std::overflow_error("Surface query coordinate overflow");
    if(scale==0)return 0;
    const auto projection=dot(divide(delta,scale),divide(edge,size));
    if(projection<=0)return 0;if(projection>=size/scale)return 1;return std::clamp(projection*scale/size,0.0,1.0);
}
SurfaceHit trianglePoint(Vector3 query,Vector3 a,Vector3 b,Vector3 c)
{
    SurfaceHit hit;const auto ab=sub(b,a),ac=sub(c,a);const auto scale=std::max(length(ab),length(ac));
    const auto u=divide(ab,scale),v=divide(ac,scale),area=cross(u,v),normal=unit(area),delta=sub(query,a);
    if(!finite(delta))throw std::overflow_error("Surface query coordinate overflow");
    const auto distanceScale=extent(delta);
    if(distanceScale==0){hit.position=a;hit.barycentric={1,0,0};hit.distance=0;return hit;}
    const auto direction=divide(delta,distanceScale);const auto projected=sub(direction,mul(normal,dot(direction,normal)));
    unsigned axis=0;if(std::abs(area.y)>std::abs(area.x))axis=1;if(std::abs(area.z)>std::abs(component(area,axis)))axis=2;
    const auto denominator=component(area,axis);
    const auto beta=component(cross(projected,v),axis)/denominator,gamma=component(cross(u,projected),axis)/denominator;
    if(std::isfinite(beta)&&std::isfinite(gamma)&&beta>=0&&gamma>=0&&beta+gamma<=scale/distanceScale)
    {
        auto bWeight=std::clamp(beta*distanceScale/scale,0.0,1.0),cWeight=std::clamp(gamma*distanceScale/scale,0.0,1.0);
        const auto total=bWeight+cWeight;if(total>1){bWeight/=total;cWeight/=total;}
        candidate(hit,query,a,b,c,{std::max(0.0,1-bWeight-cWeight),bWeight,cWeight});
    }
    auto t=segment(query,a,b);candidate(hit,query,a,b,c,{1-t,t,0});
    t=segment(query,b,c);candidate(hit,query,a,b,c,{0,1-t,t});
    t=segment(query,c,a);candidate(hit,query,a,b,c,{t,0,1-t});return hit;
}
struct ProjectionRange final
{
    double low=std::numeric_limits<double>::infinity(),high=-std::numeric_limits<double>::infinity();
};
struct ProjectionAxis final
{
    Vector3 direction;
    ProjectionRange range;
    double lengthUpper=0;
};
bool boundedProjectionTriangle(const std::array<Vector3,3> &triangle)
{for(auto p:triangle)if(extent(p)>1e100)return false;return true;}
ProjectionRange projectTriangle(const std::array<Vector3,3> &triangle,Vector3 axis)
{
    ProjectionRange result;constexpr auto epsilon=128*std::numeric_limits<double>::epsilon();
    for(auto p:triangle)
    {
        const auto value=dot(p,axis),error=epsilon*(std::abs(p.x*axis.x)+std::abs(p.y*axis.y)+std::abs(p.z*axis.z))+8*std::numeric_limits<double>::denorm_min();
        if(!std::isfinite(value)||!std::isfinite(error))return {-std::numeric_limits<double>::infinity(),std::numeric_limits<double>::infinity()};
        result.low=std::min(result.low,std::nextafter(value-error,-std::numeric_limits<double>::infinity()));
        result.high=std::max(result.high,std::nextafter(value+error,std::numeric_limits<double>::infinity()));
    }
    return result;
}
ProjectionAxis triangleProjectionAxis(const std::array<Vector3,3> &triangle)
{
    if(!boundedProjectionTriangle(triangle))return {};const auto u=sub(triangle[1],triangle[0]),v=sub(triangle[2],triangle[0]);const auto scale=std::max(length(u),length(v));
    // Preserve legacy pair-kernel overflow failures for microscopic edge pairs.
    if(std::min({length(u),length(v),length(sub(triangle[2],triangle[1]))})<1e-100)return {};
    const auto normal=cross(divide(u,scale),divide(v,scale));const auto size=length(normal);if(!(size>0)||!std::isfinite(size))return {};
    const auto direction=divide(normal,size);return {direction,projectTriangle(triangle,direction),std::nextafter(length(direction),std::numeric_limits<double>::infinity())*(1+128*std::numeric_limits<double>::epsilon())};
}
bool projectionSeparates(const ProjectionAxis &axis,const std::array<Vector3,3> &other,double distance)
{
    if(!(axis.lengthUpper>0)||!std::isfinite(distance)||!boundedProjectionTriangle(other))return false;const auto range=projectTriangle(other,axis.direction);
    if(!std::isfinite(axis.range.low)||!std::isfinite(axis.range.high)||!std::isfinite(range.low)||!std::isfinite(range.high))return false;
    const auto gap=std::max({axis.range.low-range.high,range.low-axis.range.high,0.});
    return gap>distance*axis.lengthUpper*(1+128*std::numeric_limits<double>::epsilon())+16*std::numeric_limits<double>::denorm_min();
}

struct Bounds final
{
    Vector3 low{std::numeric_limits<double>::infinity(),std::numeric_limits<double>::infinity(),std::numeric_limits<double>::infinity()};
    Vector3 high{-std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()};
    void include(Vector3 p){low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};}
    double distance(Vector3 p)const
    {auto d=[](double v,double a,double b){return v<a?a-v:v>b?v-b:0.;};return std::hypot(d(p.x,low.x,high.x),d(p.y,low.y,high.y),d(p.z,low.z,high.z));}
    double distance(const Bounds &other) const
    {auto gap=[](double a,double b,double c,double d){return c>b?c-b:a>d?a-d:0.;};return std::hypot(gap(low.x,high.x,other.low.x,other.high.x),gap(low.y,high.y,other.low.y,other.high.y),gap(low.z,high.z,other.low.z,other.high.z));}
};
}
struct MeshSurface::Impl final
{
    struct Node final{Bounds bounds;std::size_t begin=0,count=0,left=NoBone,right=NoBone;};
    std::vector<Vector3> points,centres;
    std::vector<std::uint32_t> triangles,order;
    std::vector<Node> nodes;
    std::size_t build(std::size_t begin,std::size_t end)
    {
        const auto id=nodes.size();nodes.emplace_back();Bounds bounds,centresBounds;
        for(auto i=begin;i<end;++i){const auto t=order[i];centresBounds.include(centres[t]);for(unsigned c=0;c<3;++c)bounds.include(points[triangles[t*3+c]]);}
        nodes[id].bounds=bounds;nodes[id].begin=begin;nodes[id].count=end-begin;
        if(end-begin<=8)return id;
        const auto span=sub(centresBounds.high,centresBounds.low);unsigned axis=span.y>span.x?1:0;if(span.z>component(span,axis))axis=2;
        const auto middle=begin+(end-begin)/2;
        std::nth_element(order.begin()+begin,order.begin()+middle,order.begin()+end,[&](auto a,auto b){const auto x=component(centres[a],axis),y=component(centres[b],axis);return x!=y?x<y:a<b;});
        const auto left=build(begin,middle),right=build(middle,end);nodes[id].left=left;nodes[id].right=right;nodes[id].count=0;return id;
    }
};
MeshSurface::MeshSurface(std::span<const Vector3> positions,std::span<const std::uint32_t> triangles):impl_(std::make_unique<Impl>())
{
    if(positions.empty()||positions.size()>=NoTopologyIndex||triangles.empty()||triangles.size()%3||triangles.size()/3>=NoTopologyIndex)
        throw std::invalid_argument("Invalid surface query dimensions");
    for(const auto p:positions)if(!finite(p))throw std::invalid_argument("Nonfinite surface position");
    impl_->points.assign(positions.begin(),positions.end());impl_->triangles.assign(triangles.begin(),triangles.end());
    for(std::size_t i=0;i<triangles.size();i+=3)
    {
        const auto a=triangles[i],b=triangles[i+1],c=triangles[i+2];if(a>=positions.size()||b>=positions.size()||c>=positions.size())throw std::out_of_range("Surface triangle vertex");
        const auto u=sub(positions[b],positions[a]),v=sub(positions[c],positions[a]);const auto scale=std::max(length(u),length(v));
        if(!(scale>0)||!std::isfinite(scale)||!finite(u)||!finite(v)||length(cross(divide(u,scale),divide(v,scale)))==0)throw std::invalid_argument("Degenerate surface triangle");
        impl_->centres.push_back(add(add(divide(positions[a],3),divide(positions[b],3)),divide(positions[c],3)));
    }
    impl_->order.resize(triangles.size()/3);std::iota(impl_->order.begin(),impl_->order.end(),0u);impl_->nodes.reserve(impl_->order.size()*2);impl_->build(0,impl_->order.size());
}
MeshSurface::~MeshSurface()=default;
MeshSurface::MeshSurface(MeshSurface&&) noexcept=default;
MeshSurface &MeshSurface::operator=(MeshSurface&&) noexcept=default;
SurfaceHit MeshSurface::closest(Vector3 position,double maximumDistance) const
{
    SurfaceQueryBudget budget;budget.maximumNodeVisits=budget.maximumTriangleTests=std::numeric_limits<std::size_t>::max();
    return closest(position,maximumDistance,budget);
}
SurfaceHit MeshSurface::closest(Vector3 position,double maximumDistance,SurfaceQueryBudget &budget) const
{
    return closestImpl(position,maximumDistance,budget,nullptr,0);
}
SurfaceHit MeshSurface::closestCompatible(Vector3 position,Vector3 normal,double minimumNormalCosine,
                                           double maximumDistance,SurfaceQueryBudget &budget) const
{
    if(!impl_)throw std::logic_error("Moved surface query snapshot");
    if(!std::isfinite(minimumNormalCosine)||minimumNormalCosine< -1||minimumNormalCosine>1)
        throw std::invalid_argument("Invalid surface normal threshold");
    normal=unit(normal);
    return closestImpl(position,maximumDistance,budget,&normal,minimumNormalCosine);
}
SurfaceHit MeshSurface::closestImpl(Vector3 position,double maximumDistance,SurfaceQueryBudget &budget,
                                    const Vector3 *normal,double minimumNormalCosine) const
{
    if(!impl_)throw std::logic_error("Moved surface query snapshot");
    if(!finite(position)||std::isnan(maximumDistance)||maximumDistance<0)throw std::invalid_argument("Invalid surface query");
    SurfaceHit best;double distance=maximumDistance;std::array<std::size_t,128> stack{};std::size_t size=1;stack[0]=0;
    while(size)
    {
        budget.visitNode();const auto &node=impl_->nodes[stack[--size]];if(node.bounds.distance(position)>distance)continue;
        if(node.count)
        {
            for(auto i=node.begin;i<node.begin+node.count;++i)
            {
                budget.testTriangle();const auto t=impl_->order[i];std::array<std::uint32_t,3> v{impl_->triangles[t*3],impl_->triangles[t*3+1],impl_->triangles[t*3+2]};
                if(normal)
                {
                    const auto a=impl_->points[v[0]],u=sub(impl_->points[v[1]],a),w=sub(impl_->points[v[2]],a);
                    const auto scale=std::max(length(u),length(w));
                    const auto faceNormal=unit(cross(divide(u,scale),divide(w,scale)));
                    if(std::clamp(dot(*normal,faceNormal),-1.0,1.0)<minimumNormalCosine)continue;
                }
                auto hit=trianglePoint(position,impl_->points[v[0]],impl_->points[v[1]],impl_->points[v[2]]);
                if(hit.distance<=distance&&(hit.distance<distance||!best.valid()||t<best.triangle)){hit.triangle=t;hit.vertices=v;best=hit;distance=hit.distance;}
            }
        }
        else
        {
            auto a=node.left,b=node.right;if(impl_->nodes[a].bounds.distance(position)>impl_->nodes[b].bounds.distance(position))std::swap(a,b);
            stack[size++]=b;stack[size++]=a;
        }
    }
    return best;
}
} // namespace iiActorObject
namespace iiActorObject
{
namespace
{
Vector3 interpolate(Vector3 a,Vector3 b,double t)
{return {std::lerp(a.x,b.x,t),std::lerp(a.y,b.y,t),std::lerp(a.z,b.z,t)};}
void pairCandidate(SurfacePairHit &best,const std::array<Vector3,3> &source,const std::array<Vector3,3> &target,
    std::array<double,3> a,std::array<double,3> b)
{
    Vector3 p,q;for(unsigned i=0;i<3;++i){p=add(p,mul(source[i],a[i]));q=add(q,mul(target[i],b[i]));}
    const auto distance=length(sub(p,q));if(!finite(p)||!finite(q)||!std::isfinite(distance))throw std::overflow_error("Triangle pair coordinate overflow");
    if(!best.valid()||distance<best.source.distance){best.source.triangle=0;best.source.barycentric=a;best.source.position=p;best.source.distance=distance;best.targetBarycentric=b;best.targetPosition=q;}
}
std::pair<double,double> segmentPair(Vector3 a,Vector3 b,Vector3 c,Vector3 d)
{
    const auto ab=sub(b,a),cd=sub(d,c);const auto scale=std::max(length(ab),length(cd));
    const auto u=divide(ab,scale),v=divide(cd,scale);const auto aa=dot(u,u),ee=dot(v,v);
    if(aa==0)return {0,segment(a,c,d)};if(ee==0)return {segment(c,a,b),0};
    const auto r=divide(sub(a,c),scale);if(!finite(r))throw std::overflow_error("Triangle pair scale overflow");
    const auto bb=dot(u,v),cc=dot(u,r),ff=dot(v,r),denominator=aa*ee-bb*bb;
    auto s=denominator>0?std::clamp((bb*ff-cc*ee)/denominator,0.,1.):0.;auto t=(bb*s+ff)/ee;
    if(t<0){t=0;s=std::clamp(-cc/aa,0.,1.);}else if(t>1){t=1;s=std::clamp((bb-cc)/aa,0.,1.);}return {s,t};
}
std::optional<std::pair<double,std::array<double,3>>> segmentFace(Vector3 a,Vector3 b,const std::array<Vector3,3> &face)
{
    const auto u=sub(face[1],face[0]),v=sub(face[2],face[0]);const auto scale=std::max(length(u),length(v));const auto normal=unit(cross(divide(u,scale),divide(v,scale)));
    const auto da=dot(sub(a,face[0]),normal),db=dot(sub(b,face[0]),normal);
    if(!std::isfinite(da)||!std::isfinite(db))throw std::overflow_error("Triangle intersection plane overflow");
    if(da==db||(da>0&&db>0)||(da<0&&db<0))return {};
    const auto divisor=std::max(std::abs(da),std::abs(db));if(divisor==0)return {};
    const auto t=std::clamp((da/divisor)/((da/divisor)-(db/divisor)),0.,1.);const auto point=interpolate(a,b,t);
    const auto hit=trianglePoint(point,face[0],face[1],face[2]);
    const auto tolerance=16*std::numeric_limits<double>::epsilon()*std::max({scale,extent(point),extent(face[0]),extent(face[1]),extent(face[2])});
    if(hit.distance>tolerance)return {};return std::pair{t,hit.barycentric};
}
SurfacePairHit trianglePair(const std::array<Vector3,3> &source,const std::array<Vector3,3> &target)
{
    SurfacePairHit best;
    for(unsigned i=0;i<3;++i)
    {
        const auto a=trianglePoint(target[i],source[0],source[1],source[2]);std::array<double,3> basis{};basis[i]=1;pairCandidate(best,source,target,a.barycentric,basis);
        const auto b=trianglePoint(source[i],target[0],target[1],target[2]);pairCandidate(best,source,target,basis,b.barycentric);
    }
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)
    {
        const auto [s,t]=segmentPair(source[i],source[(i+1)%3],target[j],target[(j+1)%3]);std::array<double,3> a{},b{};a[i]=1-s;a[(i+1)%3]=s;b[j]=1-t;b[(j+1)%3]=t;pairCandidate(best,source,target,a,b);
    }
    for(unsigned i=0;i<3;++i)
    {
        if(const auto hit=segmentFace(source[i],source[(i+1)%3],target)){std::array<double,3> bary{};bary[i]=1-hit->first;bary[(i+1)%3]=hit->first;pairCandidate(best,source,target,bary,hit->second);}
        if(const auto hit=segmentFace(target[i],target[(i+1)%3],source)){std::array<double,3> bary{};bary[i]=1-hit->first;bary[(i+1)%3]=hit->first;pairCandidate(best,source,target,hit->second,bary);}
    }
    return best;
}
}
SurfacePairHit MeshSurface::closestTriangle(const std::array<Vector3,3> &triangle,double maximumDistance) const
{SurfaceQueryBudget budget;return closestTriangle(triangle,maximumDistance,budget);}
SurfacePairHit MeshSurface::closestTriangle(const std::array<Vector3,3> &triangle,double maximumDistance,SurfaceQueryBudget &budget) const
{
    if(!impl_)throw std::logic_error("Moved surface query snapshot");
    if(std::isnan(maximumDistance)||maximumDistance<0)throw std::invalid_argument("Invalid triangle query radius");
    for(auto p:triangle)if(!finite(p))throw std::invalid_argument("Nonfinite triangle query position");
    const auto u=sub(triangle[1],triangle[0]),v=sub(triangle[2],triangle[0]);const auto scale=std::max(length(u),length(v));
    if(!(scale>0)||!std::isfinite(scale)||!finite(u)||!finite(v)||length(cross(divide(u,scale),divide(v,scale)))==0)throw std::invalid_argument("Degenerate query triangle");
    const auto queryAxis=triangleProjectionAxis(triangle);Bounds bounds;for(auto p:triangle)bounds.include(p);SurfacePairHit best;double distance=maximumDistance;std::array<std::size_t,128> stack{};std::size_t size=1;stack[0]=0;
    while(size)
    {
        budget.visitNode();const auto &node=impl_->nodes[stack[--size]];if(node.bounds.distance(bounds)>distance)continue;
        if(node.count)
        {
            for(auto i=node.begin;i<node.begin+node.count;++i)
            {
                budget.testTriangle();const auto t=impl_->order[i];const std::array<std::uint32_t,3> ids{impl_->triangles[t*3],impl_->triangles[t*3+1],impl_->triangles[t*3+2]};
                const std::array<Vector3,3> source{impl_->points[ids[0]],impl_->points[ids[1]],impl_->points[ids[2]]};
                if(std::isfinite(distance)&&(projectionSeparates(queryAxis,source,distance)||projectionSeparates(triangleProjectionAxis(source),triangle,distance)))continue;
                auto hit=trianglePair(source,triangle);
                if(hit.source.distance<=distance&&(hit.source.distance<distance||!best.valid()||t<best.source.triangle))
                {hit.source.triangle=t;hit.source.vertices=ids;best=hit;distance=hit.source.distance;}
            }
        }
        else
        {
            auto a=node.left,b=node.right;if(impl_->nodes[a].bounds.distance(bounds)>impl_->nodes[b].bounds.distance(bounds))std::swap(a,b);stack[size++]=b;stack[size++]=a;
        }
    }
    return best;
}
} // namespace iiActorObject
