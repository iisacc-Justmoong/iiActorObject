#include "Object3DGlbPrimitivePlan.h"
#include "Object3DMaterial.h"
#include "Object3DMesh.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace iiActorObject::object3d_detail::glb_export
{
GlbPrimitivePlan planGlbPrimitives(const Object3DMesh &mesh,std::span<const Object3DMaterial> materials)
{
    const auto &g=mesh.geometry;
    if(g.vertices.size()>std::numeric_limits<std::uint32_t>::max())throw std::overflow_error("glTF vertex limit");
    if(g.triangles.size()%3)throw std::invalid_argument("GLB primitive indices are not triangles");
    if(g.triangles.size()>std::numeric_limits<std::uint32_t>::max())throw std::overflow_error("glTF corner limit");
    if(!mesh.triangleMaterials.empty()&&mesh.triangleMaterials.size()!=g.triangles.size()/3)
        throw std::invalid_argument("GLB triangle material count mismatch");
    GlbPrimitivePlan result;
    const auto *topology=mesh.topology?&*mesh.topology:nullptr;
    const bool hasAuthored=topology&&topology->sourceGlbTangents;
    const bool authoredValid=hasAuthored&&topologyGlbTangentsValid(*topology);
    if(authoredValid)
    {
        result.tangentByLayout.assign(topology->layout->definition().layouts.size(),NoTopologyIndex);
        const auto &tangents=topology->sourceGlbTangents->tangents;
        for(std::size_t i=0;i<tangents.size();++i)
            result.tangentByLayout.at(tangents[i].layout)=static_cast<std::uint32_t>(i);
    }
    for(std::size_t i=0;i<g.triangles.size();i+=3)
    {
        const auto material=mesh.triangleMaterials.empty()?mesh.material:mesh.triangleMaterials[i/3];
        if(material>=materials.size())throw std::out_of_range("GLB triangle material");
        for(std::size_t corner=0;corner<3;++corner)
            if(g.triangles[i+corner]>=g.vertices.size())throw std::out_of_range("GLB triangle vertex");
        bool useAuthored=false;std::string_view status;
        if(hasAuthored&&!authoredValid)status="source_tangents_stale";
        if(authoredValid)
        {
            unsigned present=0;const auto &definition=topology->layout->definition();
            for(std::size_t corner=0;corner<3;++corner)
            {
                const auto layout=definition.renderToLayout.at(g.triangles[i+corner]);
                present+=result.tangentByLayout.at(layout)!=NoTopologyIndex;
            }
            if(present!=0&&present!=3)throw std::invalid_argument("Incomplete authored GLB tangent triangle");
            if(present==3)
            {
                const auto &bindings=topology->sourceGlbTangents->bindings;
                const auto binding=std::lower_bound(bindings.begin(),bindings.end(),material,
                    [](const auto &entry,std::size_t id){return entry.material<id;});
                const auto &current=materials[material];
                if(binding==bindings.end()||binding->material!=material||binding->normalMapped!=current.normalImage.has_value()||
                   (binding->normalMapped&&binding->normalUvSet!=current.normalUvSet))status="source_tangent_uv_changed";
                else useAuthored=true;
            }
        }
        auto &part=result.parts[{material,useAuthored}];if(!status.empty())part.authoredStatus=status;
        part.vertices.insert(part.vertices.end(),g.triangles.begin()+static_cast<std::ptrdiff_t>(i),
                             g.triangles.begin()+static_cast<std::ptrdiff_t>(i+3));
        for(std::size_t corner=0;corner<3;++corner)part.corners.push_back(static_cast<std::uint32_t>(i+corner));
    }
    return result;
}
} // namespace iiActorObject::object3d_detail::glb_export
