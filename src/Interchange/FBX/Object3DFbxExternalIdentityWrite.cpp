#include "Object3DFbxExternalIdentityWrite.h"
#include "Object3DExternalIdentity.h"
#include "Object3DExternalProtocol.h"
#include "Object3DFbxSceneNodes.h"
#include "Object3DMesh.h"
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace iiActorObject::object3d_detail
{
using namespace fbx_scene_nodes;
fbx_binary::Node fbxExternalDocumentProperty(std::string_view document)
{return externalText("iiActorObjectDocument",document);}
fbx_binary::Node fbxExternalBoneProperty(std::size_t sourceIndex)
{return externalInteger("iiActorObjectSourceBone",externalIndex(sourceIndex));}
std::vector<fbx_binary::Node> fbxExternalMeshProperties(std::string_view document,std::size_t meshIndex,
    const ExternalMeshIdentity &identity,const MeshTopology *source)
{
    if(source&&!source->layout)throw std::invalid_argument("Invalid FBX external identity topology");
    std::vector<fbx_binary::Node> properties;properties.reserve(6);
    properties.push_back(fbxExternalDocumentProperty(document));
    properties.push_back(externalInteger("iiActorObjectSourceMesh",externalIndex(meshIndex)));
    properties.push_back(externalText("iiActorObjectSourcePointIds",externalIds(identity.points)));
    std::vector<std::uint32_t> faceIds,cornerIds;
    if(source)
    {
        const auto &definition=source->layout->definition();
        const std::set<std::uint32_t> hidden(definition.hiddenPolygons.begin(),definition.hiddenPolygons.end());
        for(std::size_t face=0;face<definition.polygons.size();++face)
        {
            if(hidden.contains(static_cast<std::uint32_t>(face)))continue;
            faceIds.push_back(static_cast<std::uint32_t>(face));
            const auto &corners=definition.polygons[face];
            if(definition.reverseWinding)cornerIds.insert(cornerIds.end(),corners.rbegin(),corners.rend());
            else cornerIds.insert(cornerIds.end(),corners.begin(),corners.end());
        }
    }
    else{faceIds=identity.polygons;cornerIds=identity.corners;}
    properties.push_back(externalText("iiActorObjectSourcePolygonIds",externalIds(faceIds)));
    properties.push_back(externalText("iiActorObjectSourceCornerLayouts",externalIds(cornerIds)));
    std::vector<std::pair<std::string,std::string>> names;names.reserve(identity.morphs.size());
    for(const auto &morph:identity.morphs)names.push_back({morph.source,morph.source});
    properties.push_back(externalText("iiActorObjectSourceMorphs",externalMorphs(names)));
    return properties;
}
}
