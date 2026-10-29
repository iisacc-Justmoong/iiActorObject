#include "Object3DPointCorrespondence.h"
#include "CharacterMath.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace iiActorObject::object3d_detail
{
namespace
{
using namespace character_detail;
constexpr std::size_t MaximumCandidateComparisons=32'000'000;
constexpr std::size_t MaximumUnorderedPolygons=2'000'000;
constexpr std::size_t MaximumUnorderedCorners=8'000'000;
constexpr std::size_t MaximumUnorderedTriangles=2'000'000;

bool samePointTopology(const Object3DMesh &a,const Object3DMesh &b)
{
    if(!a.topology||!b.topology||!a.topology->layout||!b.topology->layout||a.topology->points.size()!=b.topology->points.size())return false;
    const auto &x=a.topology->layout->definition(),&y=b.topology->layout->definition();
    if(x.polygons.size()!=y.polygons.size())return false;
    for(std::size_t face=0;face<x.polygons.size();++face)
    {
        if(x.polygons[face].size()!=y.polygons[face].size())return false;
        for(std::size_t corner=0;corner<x.polygons[face].size();++corner)
            if(x.layouts.at(x.polygons[face][corner]).position!=y.layouts.at(y.polygons[face][corner]).position)return false;
    }
    return true;
}
std::optional<PointTopologyCorrespondence> orderedMapping(const MeshTopology &a,const MeshTopology &b,double maximumDistance)
{
    const auto &x=a.layout->definition(),&y=b.layout->definition();
    if(x.polygons.size()!=y.polygons.size())return {};
    PointTopologyCorrespondence result;
    result.sourceToTarget.assign(a.points.size(),NoTopologyIndex);result.targetToSource.assign(b.points.size(),NoTopologyIndex);
    for(std::size_t face=0;face<x.polygons.size();++face)
    {
        if(x.polygons[face].size()!=y.polygons[face].size())return {};
        for(std::size_t corner=0;corner<x.polygons[face].size();++corner)
        {
            const auto source=x.layouts.at(x.polygons[face][corner]).position;
            const auto target=y.layouts.at(y.polygons[face][corner]).position;
            auto &forward=result.sourceToTarget.at(source);auto &reverse=result.targetToSource.at(target);
            if((forward!=NoTopologyIndex&&forward!=target)||(reverse!=NoTopologyIndex&&reverse!=source))return {};
            forward=target;reverse=source;
        }
    }
    for(std::size_t source=0;source<result.sourceToTarget.size();++source)
    {
        const auto target=result.sourceToTarget[source];if(target==NoTopologyIndex)return {};
        const auto distance=length(sub(a.points[source].position,b.points[target].position));
        if(!std::isfinite(distance)||distance>maximumDistance)return {};
    }
    if(std::find(result.targetToSource.begin(),result.targetToSource.end(),NoTopologyIndex)!=result.targetToSource.end())return {};
    return result;
}
// Booth's algorithm finds the lexicographically least cyclic rotation in O(n).
std::size_t leastRotation(std::span<const std::uint32_t> points)
{
    const auto n=points.size();std::size_t a=0,b=1,offset=0;
    while(a<n&&b<n&&offset<n)
    {
        const auto x=points[(a+offset)%n],y=points[(b+offset)%n];
        if(x==y){++offset;continue;}
        if(x>y){a+=offset+1;if(a==b)++a;}else{b+=offset+1;if(a==b)++b;}
        offset=0;
    }
    return std::min(a,b);
}
struct PolygonSignature final
{
    bool hidden=false;
    std::vector<std::uint32_t> points;
    bool operator==(const PolygonSignature &) const=default;
    bool operator<(const PolygonSignature &other) const
    {return hidden!=other.hidden?hidden<other.hidden:points<other.points;}
};
std::optional<std::vector<PolygonSignature>> polygonSignatures(const MeshTopology &topology,
    const std::vector<std::uint32_t> *mapping,std::vector<bool> &used)
{
    const auto &d=topology.layout->definition();
    if(d.polygons.size()>MaximumUnorderedPolygons)return {};
    std::vector<PolygonSignature> result;result.reserve(d.polygons.size());std::size_t corners=0;
    for(std::size_t i=0;i<d.polygons.size();++i)
    {
        const auto &face=d.polygons[i];if(face.size()>MaximumUnorderedCorners-corners)return {};
        corners+=face.size();PolygonSignature item;
        item.hidden=std::binary_search(d.hiddenPolygons.begin(),d.hiddenPolygons.end(),static_cast<std::uint32_t>(i));
        item.points.reserve(face.size());
        for(const auto layout:face)
        {
            const auto point=d.layouts.at(layout).position;used.at(point)=true;
            item.points.push_back(mapping?mapping->at(point):point);
        }
        if(d.reverseWinding)std::reverse(item.points.begin(),item.points.end());
        const auto rotation=leastRotation(item.points);
        std::rotate(item.points.begin(),item.points.begin()+rotation,item.points.end());result.push_back(std::move(item));
    }
    std::sort(result.begin(),result.end());return result;
}
std::optional<PointTopologyCorrespondence> unorderedMapping(const MeshTopology &a,const MeshTopology &b,double maximumDistance)
{
    const auto polygonCount=a.layout->definition().polygons.size();
    if(polygonCount!=b.layout->definition().polygons.size()||polygonCount>MaximumUnorderedPolygons)return {};
    std::vector<std::uint32_t> order(b.points.size());std::iota(order.begin(),order.end(),0u);
    for(const auto &point:a.points)if(!finite(point.position))return {};
    for(const auto &point:b.points)if(!finite(point.position))return {};
    std::sort(order.begin(),order.end(),[&](auto x,auto y)
        {return b.points[x].position.x!=b.points[y].position.x?b.points[x].position.x<b.points[y].position.x:x<y;});
    PointTopologyCorrespondence result;
    result.sourceToTarget.assign(a.points.size(),NoTopologyIndex);result.targetToSource.assign(b.points.size(),NoTopologyIndex);
    std::size_t comparisons=0;
    for(std::size_t source=0;source<a.points.size();++source)
    {
        const auto p=a.points[source].position;
        const auto low=static_cast<long double>(p.x)-static_cast<long double>(maximumDistance);
        const auto high=static_cast<long double>(p.x)+static_cast<long double>(maximumDistance);
        auto candidate=std::lower_bound(order.begin(),order.end(),low,
            [&](auto index,long double x){return static_cast<long double>(b.points[index].position.x)<x;});
        std::uint32_t match=NoTopologyIndex;
        for(;candidate!=order.end()&&static_cast<long double>(b.points[*candidate].position.x)<=high;++candidate)
        {
            if(comparisons>=MaximumCandidateComparisons)return {};++comparisons;
            const auto distance=length(sub(p,b.points[*candidate].position));
            if(!std::isfinite(distance)||distance>maximumDistance)continue;
            if(match!=NoTopologyIndex)return {};match=*candidate;
        }
        if(match==NoTopologyIndex||result.targetToSource[match]!=NoTopologyIndex)return {};
        result.sourceToTarget[source]=match;result.targetToSource[match]=static_cast<std::uint32_t>(source);
    }
    std::vector<bool> sourceUsed(a.points.size()),targetUsed(b.points.size());
    const auto sourceFaces=polygonSignatures(a,&result.sourceToTarget,sourceUsed);
    const auto targetFaces=polygonSignatures(b,nullptr,targetUsed);
    if(!sourceFaces||!targetFaces||*sourceFaces!=*targetFaces||
       std::find(sourceUsed.begin(),sourceUsed.end(),false)!=sourceUsed.end()||
       std::find(targetUsed.begin(),targetUsed.end(),false)!=targetUsed.end())return {};
    return result;
}
std::array<std::uint32_t,3> canonicalTriangle(std::array<std::uint32_t,3> triangle)
{
    return std::min({triangle,std::array{triangle[1],triangle[2],triangle[0]},
                     std::array{triangle[2],triangle[0],triangle[1]}});
}
}
std::optional<PointTopologyCorrespondence> pointTopologyCorrespondence(const Object3DMesh &a,const Object3DMesh &b,
    const PointCorrespondenceOptions &options)
{
    if(!std::isfinite(options.maximumPointDistance)||options.maximumPointDistance<0)
        throw std::invalid_argument("Invalid reordered-point matching distance");
    const auto unmatched=[&]() -> std::optional<PointTopologyCorrespondence>
    {
        if(options.rejectUnmatched)throw std::invalid_argument("Cannot establish a unique geometry and oriented polygon correspondence");
        return {};
    };
    if(!options.uniqueGeometryOnly&&samePointTopology(a,b))return PointTopologyCorrespondence{true,{},{}};
    if(!options.uniqueGeometryOnly&&!options.inferReorderedPoints)return {};
    if(!a.topology||!b.topology||!a.topology->layout||!b.topology->layout||
       a.topology->points.size()!=b.topology->points.size()||a.topology->points.empty()||
       a.topology->points.size()>options.maximumPoints||a.topology->points.size()>=NoTopologyIndex)return unmatched();
    if(options.uniqueGeometryOnly)
    {
        auto result=unorderedMapping(*a.topology,*b.topology,options.maximumPointDistance);
        if(!result)return unmatched();
        if(!options.inferReorderedPoints)
            for(std::size_t i=0;i<result->sourceToTarget.size();++i)
                if(result->sourceToTarget[i]!=i)return unmatched();
        return result;
    }
    // The ordered path preserves its existing explicit polygon-order assumption.
    // The unordered path requires unique geometric candidates and an independent
    // full oriented-polygon connectivity match.
    if(auto ordered=orderedMapping(*a.topology,*b.topology,options.maximumPointDistance))return ordered;
    return unorderedMapping(*a.topology,*b.topology,options.maximumPointDistance);
}
bool sameVisibleTriangulation(const Object3DMesh &a,const Object3DMesh &b,const PointTopologyCorrespondence &points)
{
    if(!a.topology||!b.topology||!a.topology->layout||!b.topology->layout)return false;
    const auto &x=*a.topology->layout,&y=*b.topology->layout;
    const auto xa=x.triangles(),ya=y.triangles(),xf=x.trianglePolygons(),yf=y.trianglePolygons();
    if(xa.size()!=ya.size()||xa.size()%3||xf.size()!=yf.size())return false;
    const auto &xd=x.definition(),&yd=y.definition();
    const auto sourcePoint=[&](std::uint32_t vertex){return points.targetPoint(xd.layouts.at(xd.renderToLayout.at(vertex)).position);};
    const auto targetPoint=[&](std::uint32_t vertex){return yd.layouts.at(yd.renderToLayout.at(vertex)).position;};
    bool ordered=std::equal(xf.begin(),xf.end(),yf.begin());
    if(ordered)for(std::size_t i=0;i<xa.size();++i)if(sourcePoint(xa[i])!=targetPoint(ya[i])){ordered=false;break;}
    if(ordered)return true;
    if(xa.size()/3>MaximumUnorderedTriangles)return false;
    std::vector<std::array<std::uint32_t,3>> source,target;source.reserve(xa.size()/3);target.reserve(ya.size()/3);
    for(std::size_t i=0;i<xa.size();i+=3)
    {
        source.push_back(canonicalTriangle({sourcePoint(xa[i]),sourcePoint(xa[i+1]),sourcePoint(xa[i+2])}));
        target.push_back(canonicalTriangle({targetPoint(ya[i]),targetPoint(ya[i+1]),targetPoint(ya[i+2])}));
    }
    std::sort(source.begin(),source.end());std::sort(target.begin(),target.end());return source==target;
}
} // namespace iiActorObject::object3d_detail
