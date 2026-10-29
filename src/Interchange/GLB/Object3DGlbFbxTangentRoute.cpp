#include "Object3DGlbFbxTangentRoute.h"
#include "Object3DMesh.h"
#include <algorithm>

namespace iiActorObject::object3d_detail::glb_export
{
GlbFbxTangentSelector::GlbFbxTangentSelector(const Object3DMesh &mesh):mesh_(&mesh)
{
    const auto *topology=mesh.topology?&*mesh.topology:nullptr;
    hasSource_=topology&&topology->sourceTangents;
    valid_=hasSource_&&topologySourceTangentsValid(*topology);
}
GlbFbxTangentRoute GlbFbxTangentSelector::select(std::optional<std::size_t> normalUv)
{
    GlbFbxTangentRoute route;
    if(hasSource_&&!valid_)route.notes.push_back("source_fbx_tangents_stale");
    if(!valid_)return route;
    const auto &layers=mesh_->topology->sourceTangents->layers;
    if(normalUv)
    {
        const auto found=std::find_if(layers.begin(),layers.end(),
            [&](const auto &layer){return layer.uvSet==*normalUv;});
        if(found==layers.end())route.notes.push_back("source_fbx_uv_layer_missing");
        else if(found->tangents.empty())route.notes.push_back("source_fbx_tangent_missing");
        else route.layer=&*found;
    }
    else
    {
        for(const auto &layer:layers)
            if(!layer.tangents.empty())
            {
                if(route.layer){route.layer=nullptr;route.notes.push_back("source_fbx_uv_ambiguous");break;}
                route.layer=&layer;
            }
        if(!route.layer&&route.notes.empty())route.notes.push_back("source_fbx_tangent_missing");
        if(route.layer)route.notes.push_back("source_fbx_unbound_uv");
    }
    if(!route.layer)return route;
    if(!normalUv)route.unboundUv=route.layer->uvSet;
    if(std::any_of(layers.begin(),layers.end(),[&](const auto &layer){return layer.uvSet!=route.layer->uvSet;}))
        route.notes.push_back("source_fbx_other_uv_layers_omitted");
    if(!projectionAttempted_)
    {
        projection_=projectFbxTangentCorners(*mesh_,projectionFailure_);
        projectionAttempted_=true;
    }
    if(!projection_)route.notes.push_back(projectionFailure_);
    else route.projection=&*projection_;
    return route;
}
} // namespace iiActorObject::object3d_detail::glb_export
