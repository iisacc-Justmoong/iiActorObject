#include "Object3DFbxExternalIdentityRead.h"
#include "Object3DFbxImportFields.h"
#include "Object3DExternalProtocol.h"
#include "MeshTopologyCore.h"
#include <algorithm>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace iiActorObject::object3d_detail
{
using fbx_fields::externalIndex;
using fbx_fields::externalText;
using fbx_fields::property;

std::string readFbxExternalDocument(const ufbx_scene &scene)
{
    std::string document;
    for(const auto *node:scene.nodes)if(const auto *p=property(node->props,"iiActorObjectDocument"))
    {
        const auto token=externalText(p);
        if(token.empty()||token.size()>128||token.find('\0')!=std::string_view::npos||
           (!document.empty()&&document!=token))
            throw std::invalid_argument("Conflicting or malformed external document token");
        document=token;
    }
    return document;
}

std::optional<ExternalMeshIdentity> readFbxExternalMeshIdentity(const ufbx_node &node,
    const ufbx_mesh &source,const MeshTopology &topology,std::size_t mesh,
    bool restoredAuthoringMetadata,std::size_t maxMorphs)
{
    const auto *sourceMesh=property(node.props,"iiActorObjectSourceMesh");
    if(!sourceMesh)return std::nullopt;
    ExternalMeshIdentity identity;identity.mesh=mesh;identity.source=externalIndex(sourceMesh);
    identity.points=externalIds(externalText(property(node.props,"iiActorObjectSourcePointIds")),topology.points.size());
    identity.polygons=externalIds(externalText(property(node.props,"iiActorObjectSourcePolygonIds")),source.faces.count);
    identity.corners=externalIds(externalText(property(node.props,"iiActorObjectSourceCornerLayouts")),source.num_indices);
    if(restoredAuthoringMetadata)
    {
        const auto &d=topology.layout->definition();std::set<std::uint32_t> hidden(d.hiddenPolygons.begin(),d.hiddenPolygons.end());std::size_t f=0,c=0;
        for(std::size_t p=0;p<identity.points.size();++p)if(identity.points[p]!=p)throw std::invalid_argument("Authoring metadata and external point IDs disagree");
        for(std::size_t p=0;p<d.polygons.size();++p)if(!hidden.contains(static_cast<std::uint32_t>(p)))
        {
            if(f>=identity.polygons.size()||identity.polygons[f++]!=p)throw std::invalid_argument("Authoring metadata and external face IDs disagree");
            auto face=d.polygons[p];if(d.reverseWinding)std::reverse(face.begin(),face.end());
            for(auto layout:face)if(c>=identity.corners.size()||identity.corners[c++]!=layout)throw std::invalid_argument("Authoring metadata and external corner IDs disagree");
        }
        if(f!=identity.polygons.size()||c!=identity.corners.size())throw std::invalid_argument("External identity metadata dimensions disagree");
        identity.polygons.clear();identity.corners.clear();
        for(std::size_t p=0;p<d.polygons.size();++p)
        {identity.polygons.push_back(static_cast<std::uint32_t>(p));identity.corners.insert(identity.corners.end(),d.polygons[p].begin(),d.polygons[p].end());}
    }
    const auto names=externalMorphs(externalText(property(node.props,"iiActorObjectSourceMorphs")),maxMorphs);
    for(std::size_t m=0;m<topology.morphTargets.size();++m)
    {
        const auto found=names.find(topology.morphTargets[m].name);
        if(found==names.end())throw std::invalid_argument("Unmapped external morph channel");
        identity.morphs.push_back({m,found->second});
    }
    if(names.size()!=identity.morphs.size())throw std::invalid_argument("Stale external morph identities");
    return identity;
}
}
