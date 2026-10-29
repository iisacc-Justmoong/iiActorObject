#include "MeshSymmetry.h"
#include "MeshEditGraph.h"
#include "CharacterMath.h"
#include <numeric>

namespace iiActorObject
{
using namespace character_detail;
namespace
{
std::size_t pointCount(const Object3DMesh &mesh)
{return mesh.topology?mesh.topology->points.size():mesh.geometry.vertices.size();}
void optionsValid(const Object3DMesh &mesh,const MeshSymmetryOptions &o)
{
    if(!finite(o.plane.normal)||!std::isfinite(o.plane.offset)||!std::isfinite(o.maximumDistance)||o.maximumDistance<0||!std::isfinite(o.centreTolerance)||o.centreTolerance<0||o.unmatched<MeshSymmetryUnmatched::Reject||o.unmatched>MeshSymmetryUnmatched::Keep)
        throw std::invalid_argument("Invalid mesh symmetry options");
    (void)unit(o.plane.normal);
    if(!o.maximumPoints||pointCount(mesh)>o.maximumPoints||!o.maximumPointVisits)throw std::length_error("Mesh symmetry point budget");
}
std::vector<std::uint32_t> connectivity(const Object3DMesh &mesh)
{
    std::vector<std::uint32_t> result;
    const auto count=[&](std::size_t value){if(value>=NoTopologyIndex)throw std::length_error("Symmetry connectivity limit");result.push_back(static_cast<std::uint32_t>(value));};
    count(pointCount(mesh));result.push_back(mesh.topology?1:0);
    if(mesh.topology)
    {
        if(!mesh.topology->layout)throw std::invalid_argument("Missing symmetry topology layout");
        const auto &d=mesh.topology->layout->definition();count(d.polygons.size());
        for(const auto &polygon:d.polygons){count(polygon.size());for(auto layout:polygon)result.push_back(d.layouts.at(layout).position);}
        auto hidden=d.hiddenPolygons;std::sort(hidden.begin(),hidden.end());count(hidden.size());result.insert(result.end(),hidden.begin(),hidden.end());
    }
    else{count(mesh.geometry.triangles.size());result.insert(result.end(),mesh.geometry.triangles.begin(),mesh.geometry.triangles.end());}
    return result;
}
Vector3 reflected(Vector3 value,BrushMirrorPlane plane,bool point)
{
    if(!finite(value))throw std::invalid_argument("Nonfinite symmetry input");
    const auto distance=dot(value,plane.normal)-(point?plane.offset:0);
    const auto result=sub(value,mul(plane.normal,2*distance));
    if(!finite(result))throw std::invalid_argument("Unrepresentable symmetry reflection");return result;
}
std::int8_t side(Vector3 p,BrushMirrorPlane plane,double tolerance)
{
    const auto distance=dot(p,plane.normal)-plane.offset;if(!std::isfinite(distance))throw std::invalid_argument("Unrepresentable symmetry side");
    return distance < -tolerance ? -1 : distance > tolerance ? 1 : 0;
}
double coordinate(Vector3 p,unsigned axis){return axis==0?p.x:axis==1?p.y:p.z;}
class PointTree final
{
    static constexpr auto none=std::numeric_limits<std::size_t>::max();
    struct Node final{std::uint32_t point;unsigned axis;std::size_t left=none,right=none;};
    std::span<const Vector3> points_;
    std::vector<std::uint32_t> order_;
    std::vector<Node> nodes_;
    std::size_t root_=none;
    std::size_t build(std::size_t begin,std::size_t end)
    {
        if(begin==end)return none;auto low=points_[order_[begin]],high=low;
        for(auto i=begin+1;i<end;++i){auto p=points_[order_[i]];low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};}
        const auto extent=sub(high,low);unsigned axis=extent.y>extent.x?1:0;if(extent.z>coordinate(extent,axis))axis=2;const auto middle=begin+(end-begin)/2;
        std::nth_element(order_.begin()+begin,order_.begin()+middle,order_.begin()+end,[&](auto a,auto b){const auto x=coordinate(points_[a],axis),y=coordinate(points_[b],axis);return x!=y?x<y:a<b;});
        const auto index=nodes_.size();nodes_.push_back({order_[middle],axis});const auto left=build(begin,middle),right=build(middle+1,end);nodes_[index].left=left;nodes_[index].right=right;return index;
    }
    void query(std::size_t index,Vector3 p,double &distance,std::uint32_t &best,bool &ambiguous,std::size_t &visits,std::size_t budget) const
    {
        if(index==none)return;if(visits==budget)throw std::length_error("Mesh symmetry search visit budget");++visits;
        const auto &node=nodes_[index];const auto delta=sub(p,points_[node.point]);const auto d=length(delta);
        const auto tolerance=16*std::numeric_limits<double>::epsilon()*std::max({d,distance,std::numeric_limits<double>::min()});
        if(d<=distance)
        {
            if(best==NoTopologyIndex||d<distance-tolerance){distance=d;best=node.point;ambiguous=false;}
            else if(node.point!=best&&std::abs(d-distance)<=tolerance)ambiguous=true;
        }
        auto first=node.left,second=node.right;const auto split=coordinate(delta,node.axis);if(split>0)std::swap(first,second);
        query(first,p,distance,best,ambiguous,visits,budget);if(std::abs(split)<=distance)query(second,p,distance,best,ambiguous,visits,budget);
    }
  public:
    PointTree(std::span<const Vector3> points,std::vector<std::uint32_t> indices):points_(points),order_(std::move(indices))
    {nodes_.reserve(order_.size());root_=build(0,order_.size());}
    std::pair<std::uint32_t,bool> closest(Vector3 p,double distance,std::size_t &visits,std::size_t budget) const
    {std::uint32_t best=NoTopologyIndex;bool ambiguous=false;query(root_,p,distance,best,ambiguous,visits,budget);return {best,ambiguous};}
};
Vector3 blendVector(Vector3 a,Vector3 b,double t)
{return {std::lerp(a.x,b.x,t),std::lerp(a.y,b.y,t),std::lerp(a.z,b.z,t)};}
}
MeshSymmetryMap MeshSymmetryMap::create(const Object3DMesh &mesh,const MeshSymmetryOptions &o)
{
    optionsValid(mesh,o);object3d_detail::MeshEditGraph graph(mesh);const BrushMirrorPlane plane{unit(o.plane.normal),o.plane.offset};
    std::vector<std::int8_t> sides;std::vector<std::uint32_t> negative,positive,raw(graph.positions.size(),NoTopologyIndex);
    for(std::size_t i=0;i<graph.positions.size();++i)
    {
        sides.push_back(side(graph.positions[i],plane,o.centreTolerance));if(!o.includeUnusedPoints&&!graph.visible[i])continue;
        if(sides.back()<0)negative.push_back(static_cast<std::uint32_t>(i));else if(sides.back()>0)positive.push_back(static_cast<std::uint32_t>(i));else raw[i]=static_cast<std::uint32_t>(i);
    }
    PointTree negativeTree(graph.positions,negative),positiveTree(graph.positions,positive);std::size_t visits=0;
    const auto match=[&](std::span<const std::uint32_t> ids,const PointTree &tree)
    {
        for(auto id:ids)
        {
            const auto [nearest,ambiguous]=tree.closest(reflected(graph.positions[id],plane,true),o.maximumDistance,visits,o.maximumPointVisits);
            if(ambiguous&&o.unmatched==MeshSymmetryUnmatched::Reject)throw std::invalid_argument("Ambiguous symmetry point; provide explicit pairs");
            if(!ambiguous)raw[id]=nearest;
        }
    };
    match(negative,positiveTree);match(positive,negativeTree);auto pairs=raw;
    for(std::size_t i=0;i<raw.size();++i)if(raw[i]!=NoTopologyIndex&&raw[raw[i]]!=i)pairs[i]=NoTopologyIndex;
    auto result=fromPairs(mesh,pairs,o);result.pointVisits_=visits;return result;
}
MeshSymmetryMap MeshSymmetryMap::fromPairs(const Object3DMesh &mesh,std::span<const std::uint32_t> pairs,const MeshSymmetryOptions &o)
{
    optionsValid(mesh,o);object3d_detail::MeshEditGraph graph(mesh);if(pairs.size()!=graph.positions.size())throw std::invalid_argument("Symmetry correspondence dimensions");
    MeshSymmetryMap result;result.plane_={unit(o.plane.normal),o.plane.offset};result.connectivity_=connectivity(mesh);result.mirrors_.assign(pairs.begin(),pairs.end());
    for(auto p:graph.positions)result.sides_.push_back(side(p,result.plane_,o.centreTolerance));
    for(std::size_t i=0;i<pairs.size();++i)
    {
        const auto j=pairs[i];const bool eligible=o.includeUnusedPoints||graph.visible[i];
        if(j==NoTopologyIndex){if(eligible&&o.unmatched==MeshSymmetryUnmatched::Reject)throw std::out_of_range("No mutual symmetry counterpart within the search limit");continue;}
        if(!eligible||j>=pairs.size()||pairs[j]!=i||(result.sides_[i]==0?j!=i:result.sides_[i]*result.sides_[j]!=-1))throw std::invalid_argument("Symmetry pairs must form an opposite-side involution");
        const auto distance=length(sub(graph.positions[j],result.reflectPoint(graph.positions[i])));if(!std::isfinite(distance))throw std::invalid_argument("Unrepresentable symmetry correspondence distance");
        result.maximumMatchDistance_=std::max(result.maximumMatchDistance_,distance);++result.mappedPoints_;
    }
    return result;
}
std::span<const std::uint32_t> MeshSymmetryMap::mirrors() const noexcept{return mirrors_;}
std::span<const std::int8_t> MeshSymmetryMap::sides() const noexcept{return sides_;}
const BrushMirrorPlane &MeshSymmetryMap::plane() const noexcept{return plane_;}
std::size_t MeshSymmetryMap::mappedPointCount() const noexcept{return mappedPoints_;}
double MeshSymmetryMap::maximumMatchDistance() const noexcept{return maximumMatchDistance_;}
std::size_t MeshSymmetryMap::pointVisits() const noexcept{return pointVisits_;}
void MeshSymmetryMap::validateFor(const Object3DMesh &mesh) const
{
    if(connectivity(mesh)!=connectivity_)throw std::invalid_argument("Symmetry source IDs/connectivity changed");
    object3d_detail::MeshEditGraph graph(mesh);if(graph.positions.size()!=mirrors_.size())throw std::invalid_argument("Symmetry point dimensions changed");
}
Vector3 MeshSymmetryMap::reflectPoint(Vector3 value) const{return reflected(value,plane_,true);}
Vector3 MeshSymmetryMap::reflectVector(Vector3 value) const{return reflected(value,plane_,false);}
MeshSymmetryResult symmetrizeMesh(Object3DMesh &mesh,const MeshSymmetryMap &mapping,const MeshSymmetryEditOptions &o)
{
    mapping.validateFor(mesh);weight(o.strength);if(!std::isfinite(o.epsilon)||o.epsilon<0||o.direction<MeshSymmetryDirection::NegativeToPositive||o.direction>MeshSymmetryDirection::Average)throw std::invalid_argument("Invalid symmetry edit options");
    const auto pairs=mapping.mirrors();if(!o.mask.empty())object3d_detail::validatePointWeights(o.mask,pairs.size());
    object3d_detail::MeshEditGraph graph(mesh);MeshSymmetryResult report;auto next=mesh;
    for(std::size_t i=0;i<pairs.size();++i)
    {
        const auto j=pairs[i];if(j==NoTopologyIndex){++report.unmappedPoints;continue;}const auto side=mapping.sides()[i];
        if((o.direction==MeshSymmetryDirection::NegativeToPositive&&side<0)||(o.direction==MeshSymmetryDirection::PositiveToNegative&&side>0))continue;
        const auto strength=o.strength*(o.mask.empty()?1:o.mask[i]);if(strength==0)continue;
        auto target=mapping.reflectPoint(graph.positions[j]);if(j==i||o.direction==MeshSymmetryDirection::Average)target=blendVector(graph.positions[i],target,.5);
        const auto value=blendVector(graph.positions[i],target,strength);if(!finite(value))throw std::invalid_argument("Unrepresentable symmetry position");
        if(length(sub(value,graph.positions[i]))<=o.epsilon)continue;
        if(next.topology)next.topology->points[i].position=value;else next.geometry.vertices[i].position=value;++report.changedPoints;
    }
    if(report.changedPoints){object3d_detail::refreshEditedMesh(next,true);mesh=std::move(next);}return report;
}
} // namespace iiActorObject
