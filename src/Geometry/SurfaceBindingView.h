#pragma once
// Private, non-owning view over topology points or render vertices.
#include "Object3DData.h"
#include <stdexcept>

namespace iiActorObject::surface_detail
{
struct MeshView final
{
    const Object3DMesh &mesh;
    explicit MeshView(const Object3DMesh &value):mesh(value){if(mesh.topology&&!mesh.topology->layout)throw std::invalid_argument("Missing surface source layout");}
    std::size_t count()const{return mesh.topology?mesh.topology->points.size():mesh.geometry.vertices.size();}
    std::size_t vertices()const{return mesh.topology?mesh.topology->layout->definition().renderToLayout.size():mesh.geometry.vertices.size();}
    Vector3 point(std::size_t p)const{return mesh.topology?mesh.topology->points.at(p).position:mesh.geometry.vertices.at(p).position;}
    Vector3 vertex(std::size_t p)const
    {
        if(!mesh.topology)return point(p);const auto &d=mesh.topology->layout->definition();return point(d.layouts.at(d.renderToLayout.at(p)).position);
    }
    std::size_t sourcePoint(std::size_t vertex)const
    {if(!mesh.topology)return vertex;const auto &d=mesh.topology->layout->definition();return d.layouts.at(d.renderToLayout.at(vertex)).position;}
    std::span<const SkinWeight> weights(std::size_t p)const
    {return mesh.topology?std::span<const SkinWeight>(mesh.topology->points.at(p).weights):std::span<const SkinWeight>(mesh.geometry.vertices.at(p).weights);}
    std::span<const std::uint32_t> triangles()const{return mesh.topology?mesh.topology->layout->triangles():std::span<const std::uint32_t>(mesh.geometry.triangles);}
};
} // namespace iiActorObject::surface_detail
