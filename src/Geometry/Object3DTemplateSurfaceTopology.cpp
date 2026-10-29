#include "Object3DTemplateSurfaceTopology.h"
#include "Object3DMesh.h"
#include "SurfaceBindingCore.h"
#include <algorithm>
#include <array>
#include <map>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail
{
void validateTemplateVisibilityLayout(const MeshTopology &old,const MeshTopology &next,bool ignoreMaterials)
{
    if(!old.layout||!next.layout||old.points.size()!=next.points.size())throw std::invalid_argument("Template visibility source points changed");
    auto left=old.layout->definition(),right=next.layout->definition();
    left.hiddenPolygons.clear();right.hiddenPolygons.clear();left.renderToLayout.clear();right.renderToLayout.clear();
    if(ignoreMaterials){left.polygonMaterials.clear();right.polygonMaterials.clear();}
    if(left!=right)throw std::invalid_argument("Template visibility source topology changed");
    constexpr std::size_t limit=2'000'000;
    if(left.polygons.size()>limit||old.layout->trianglePolygons().size()>limit||next.layout->trianglePolygons().size()>limit)
        throw std::length_error("Template visibility comparison budget");
    using Triangle=std::array<std::uint32_t,3>;
    const auto collect=[&](const MeshTopologyLayout &layout)
    {
        std::vector<std::vector<Triangle>> faces(left.polygons.size());
        const auto triangles=layout.triangles(),polygons=layout.trianglePolygons();const auto &render=layout.definition().renderToLayout;
        for(std::size_t i=0;i<polygons.size();++i)
            faces[polygons[i]].push_back({render[triangles[3*i]],render[triangles[3*i+1]],render[triangles[3*i+2]]});
        return faces;
    };
    const auto previous=collect(*old.layout),replacement=collect(*next.layout);
    for(std::size_t i=0;i<previous.size();++i)
        if(!previous[i].empty()&&!replacement[i].empty()&&previous[i]!=replacement[i])
            throw std::invalid_argument("Template visibility changes existing triangulation");
}
void validateRetainedTemplateLayout(const Object3DMesh &old,const Object3DMesh &next)
{
    if(old.name!=next.name||old.topology.has_value()!=next.topology.has_value())
        throw std::invalid_argument("Retained template mesh identity changed");
    if(!old.topology)
    {
        if(old.geometry.vertices.size()!=next.geometry.vertices.size()||old.geometry.triangles!=next.geometry.triangles)
            throw std::invalid_argument("Retained template render topology changed");
        return;
    }
    if(!old.topology->layout||!next.topology->layout||old.topology->points.size()!=next.topology->points.size())
        throw std::invalid_argument("Retained template source points changed");
    auto a=old.topology->layout->definition(),b=next.topology->layout->definition();
    a.polygonMaterials.clear();b.polygonMaterials.clear();
    const auto at=old.topology->layout->triangles(),bt=next.topology->layout->triangles();
    const auto af=old.topology->layout->trianglePolygons(),bf=next.topology->layout->trianglePolygons();
    if(a!=b||!std::equal(at.begin(),at.end(),bt.begin(),bt.end())||!std::equal(af.begin(),af.end(),bf.begin(),bf.end()))
        throw std::invalid_argument("Retained template source topology changed");
}
// Freeze the original visible support points before changing a grounded
// surface's visibility. Subsequent replacements retain the same support set.
void preserveTemplateGroundingSupport(std::vector<std::uint32_t> &supportPoints,const MeshTopology &old,const MeshTopology &next)
{
    if(!old.layout||!next.layout||old.points.size()!=next.points.size())
        throw std::invalid_argument("Template grounding source topology changed");
    if(!supportPoints.empty())return;
    const auto count=old.points.size();
    if(count>2'000'000)throw std::length_error("Template grounding comparison budget");
    std::vector<bool> previous(count),replacement(count);
    const auto mark=[](const MeshTopology &topology,std::vector<bool> &selected)
    {
        const auto &layout=topology.layout->definition();
        for(const auto index:layout.renderToLayout)selected.at(layout.layouts.at(index).position)=true;
    };
    mark(old,previous);mark(next,replacement);
    if(previous==replacement)return;
    std::vector<std::uint32_t> selected;
    for(std::size_t i=0;i<count;++i)if(previous[i])selected.push_back(static_cast<std::uint32_t>(i));
    if(selected.empty())throw std::invalid_argument("Template grounding support is empty");
    supportPoints=std::move(selected);
}
// Triangle-frame bindings use render indices, so match their original source
// polygon and layout corners after a visibility change. Control-point bindings
// already use stable source IDs and need no remapping.
void remapTemplateVisibilityBinding(SurfaceBinding &binding,const MeshTopology &old,const MeshTopology &next)
{
    if(binding.mode!=SurfaceBindingMode::TriangleFrame)return;
    if(!old.layout||!next.layout)throw std::invalid_argument("Template follower source topology is missing");
    using Key=std::array<std::uint32_t,4>;
    using Triangle=std::array<std::uint32_t,3>;
    const auto &oldLayout=*old.layout,&newLayout=*next.layout;
    const auto oldTriangles=oldLayout.triangles(),oldFaces=oldLayout.trianglePolygons();
    const auto newTriangles=newLayout.triangles(),newFaces=newLayout.trianglePolygons();
    const auto &oldRender=oldLayout.definition().renderToLayout,&newRender=newLayout.definition().renderToLayout;
    if(binding.sourceVertexCount!=oldRender.size())throw std::invalid_argument("Template follower source identity changed");
    const auto key=[](std::uint32_t face,std::span<const std::uint32_t> triangles,
        std::span<const std::uint32_t> render,std::size_t index)
    {return Key{face,render[triangles[index*3]],render[triangles[index*3+1]],render[triangles[index*3+2]]};};
    std::map<Key,std::pair<std::uint32_t,Triangle>> surviving;
    for(std::size_t i=0;i<newFaces.size();++i)
    {
        if(i>=NoTopologyIndex)throw std::length_error("Template follower triangle limit");
        const auto k=key(newFaces[i],newTriangles,newRender,i);
        surviving.try_emplace(k,std::pair{static_cast<std::uint32_t>(i),Triangle{newTriangles[3*i],newTriangles[3*i+1],newTriangles[3*i+2]}});
    }
    auto points=binding.points;
    for(auto &point:points)
    {
        if(point.triangle>=oldFaces.size())throw std::invalid_argument("Template follower triangle is invalid");
        const auto match=surviving.find(key(oldFaces[point.triangle],oldTriangles,oldRender,point.triangle));
        if(match==surviving.end())throw std::invalid_argument("Template visibility hides a follower anchor triangle");
        point.triangle=match->second.first;point.vertices=match->second.second;
    }
    binding.points=std::move(points);
    binding.sourceVertexCount=newRender.size();
}
}
