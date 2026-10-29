#include "Object3DFbxSceneMetadataWrite.h"
#include "Object3DFbxSceneNodes.h"
#include <string>

namespace iiActorObject::object3d_detail
{
using namespace fbx_binary;
using namespace fbx_scene_nodes;
void appendFbxSceneMetadata(std::vector<Node> &properties,std::vector<Node> &modelChildren,
    std::string_view name,std::int32_t kind,std::optional<std::span<const std::uint8_t>> rigCatalog)
{
    properties.push_back(externalText("iiObject3DName",name));
    properties.push_back(pInt("iiObject3DKind",kind));
    if(rigCatalog)modelChildren.push_back({"iiActorObjectRigSamples",{},
        {value("Version",prop(1)),value("Data",blob(*rigCatalog))}});
}
Node fbxBoneSemanticProperty(std::size_t index,std::string_view name)
{return externalText("iiActorSemantic."+std::to_string(index),name);}
}
