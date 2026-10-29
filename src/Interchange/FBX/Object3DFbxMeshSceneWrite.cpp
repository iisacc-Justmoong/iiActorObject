#include "Object3DFbxMeshSceneWrite.h"
#include "Object3DFbxPoseWrite.h"
#include "Object3DFbxSceneNodes.h"
#include <iterator>
#include <utility>
namespace iiActorObject::object3d_detail
{
using namespace fbx_binary;
void appendFbxMeshScene(FbxMeshSceneInput input,Node &objects,Node &connections,Node &pose)
{
    auto model=fbx_scene_nodes::model(input.modelId,input.name,"Mesh");
    auto &properties=model.children[1].children;
    properties.insert(properties.end(),std::make_move_iterator(input.identityProperties.begin()),
        std::make_move_iterator(input.identityProperties.end()));
    objects.children.push_back(std::move(model));
    const auto connect=[&](std::int64_t child,std::int64_t parent)
    {connections.children.push_back({"C",{prop("OO"),prop(child),prop(parent)}, {}});};
    connect(input.geometryId,input.modelId);connect(input.modelId,0);
    for(const auto material:input.materialIds)connect(material,input.modelId);
    appendFbxBindPoseNode(pose,input.modelId,Matrix4{});
}
}
