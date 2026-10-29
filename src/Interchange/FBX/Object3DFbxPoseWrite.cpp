#include "Object3DFbxPoseWrite.h"
#include "Object3DFbxSceneNodes.h"
#include <limits>
#include <stdexcept>
namespace iiActorObject::object3d_detail
{
using namespace fbx_binary;
Node makeFbxBindPose(std::int64_t id,std::size_t sourceBoneCount,std::size_t meshCount)
{
    constexpr auto limit=static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
    if(sourceBoneCount>=limit||meshCount>limit-1-sourceBoneCount)
        throw std::length_error("FBX bind pose node count range");
    return {"Pose",{prop(id),prop(fbx_scene_nodes::objectName("BindPose","Pose")),prop("BindPose")},
        {value("Type",prop("BindPose")),value("Version",prop(100)),
         value("NbPoseNodes",prop(static_cast<std::int32_t>(sourceBoneCount+meshCount+1)))}};
}
void appendFbxBindPoseNode(Node &pose,std::int64_t id,const Matrix4 &matrix)
{pose.children.push_back({"PoseNode",{},{value("Node",prop(id)),value("Matrix",array(matrix.elements()))}});}
}
