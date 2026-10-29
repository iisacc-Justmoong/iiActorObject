#include "Object3DFbxMeshSceneRead.h"
#include "Object3DFbxExternalIdentityRead.h"
#include "Object3DFbxMorphChannelRead.h"
#include "Object3DFbxTangentRead.h"
#include <set>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail
{
std::vector<Object3DMesh> readFbxMeshScene(const ufbx_scene &scene,
    const std::unordered_map<const ufbx_node*,BoneIndex> &bones,
    const std::unordered_map<const ufbx_material*,std::size_t> &materials,
    FbxMorphChannelReader &morphs,std::vector<ExternalMeshIdentity> &identities,
    const FbxMeshSceneReadOptions &options,const FbxWarningSink &warn)
{
    std::vector<Object3DMesh> meshes;
    std::set<std::string> meshNames;
    FbxGeometryReadBudget budget;
    std::size_t tangentCorners=0;
    for(const auto *node:scene.nodes)
    {
        if(!node->mesh)continue;
        const auto &source=*node->mesh;
        if(source.num_vertices>options.geometry.maxVertices||source.skin_deformers.count>1)
            throw std::invalid_argument("FBX mesh exceeds limits or has multiple skin deformers");
        if(source.cache_deformers.count)
            warn("Geometry cache deformers are outside the editable bind snapshot.");
        if(source.faces.count==0)
        {warn("Non-surface FBX mesh helpers were skipped; their polygon geometry is empty.");continue;}
        const auto tangentInspection=inspectFbxTangents(source);
        for(const auto &message:tangentInspection.warnings)warn(message);
        auto geometryResult=readFbxGeometry(source,*node,bones,materials,
            fbx_fields::unique(fbx_fields::string(node->name),meshNames),options.geometry,budget);
        for(auto &message:geometryResult.warnings)warn(std::move(message));
        auto &out=geometryResult.mesh;auto &topology=geometryResult.topology;
        const auto &geometry=geometryResult.geometry;const auto &extraSources=geometryResult.extraUvSets;
        const auto *meta=geometryResult.metadata;const auto corners=geometryResult.corners;
        const auto reflected=geometryResult.reflected;
        for(auto &message:morphs.readMesh(source,out,topology,meshes.size(),geometry,
            meta!=nullptr,scene.metadata.mirror_axis))warn(std::move(message));
        if(options.readExternalIdentity)
            if(auto identity=readFbxExternalMeshIdentity(*node,source,topology,meshes.size(),
                meta&&options.geometry.restoreMetadata,options.maximumMorphs))
                identities.push_back(std::move(*identity));
        for(auto &message:readFbxTangents(source,geometry,extraSources,tangentInspection,
            {corners,options.boneCount,reflected,meta&&options.geometry.restoreMetadata,scene.metadata.mirror_axis},
            tangentCorners,topology))warn(std::move(message));
        out.topology=std::move(topology);meshes.push_back(std::move(out));
    }
    morphs.finish(meshes);
    return meshes;
}
}
