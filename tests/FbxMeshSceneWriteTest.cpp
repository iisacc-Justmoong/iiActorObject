#include "../src/Interchange/FBX/Object3DFbxMeshSceneWrite.h"
#include "../src/Interchange/FBX/Object3DFbxPoseWrite.h"
#include "../src/Interchange/FBX/Object3DFbxSceneNodes.h"
#include <array>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>
using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
using namespace iiActorObject::object3d_detail::fbx_binary;
namespace
{
void check(bool value){if(!value)throw std::runtime_error("FBX mesh scene writer contract failed");}
const Node &child(const Node &node,std::string_view name)
{for(const auto &entry:node.children)if(entry.name==name)return entry;throw std::runtime_error("Missing FBX scene node");}
void edge(const Node &node,std::int64_t source,std::int64_t target)
{check(node.name=="C"&&node.properties.size()==3&&node.properties[0].bytes==prop("OO").bytes&&node.properties[1].bytes==prop(source).bytes&&node.properties[2].bytes==prop(target).bytes);}
}
int main()
{
    Node objects{"Objects",{},{Node{"Earlier",{},{}}}},connections{"Connections",{},{Node{"Earlier",{},{}}}};
    auto pose=makeFbxBindPose(7,1,2);appendFbxBindPoseNode(pose,9,Matrix4{});
    const std::array<std::int64_t,2> materials{21,22};
    std::vector<Node> fields{fbx_scene_nodes::externalText("Source","token")};
    appendFbxMeshScene({"Face",101,102,materials,std::move(fields)},objects,connections,pose);
    check(objects.children.size()==2&&objects.children[0].name=="Earlier"&&objects.children[1].name=="Model");
    const auto &model=objects.children[1];
    check(model.properties[0].bytes==prop(std::int64_t{102}).bytes&&model.properties[1].bytes==prop(fbx_scene_nodes::objectName("Face","Model")).bytes);
    const auto &properties=child(model,"Properties70").children;
    check(properties.size()==6&&properties.back().properties[0].bytes==prop("Source").bytes&&properties.back().properties[4].bytes==prop("token").bytes);
    check(connections.children.size()==5);edge(connections.children[1],101,102);edge(connections.children[2],102,0);
    edge(connections.children[3],21,102);edge(connections.children[4],22,102);
    check(pose.children.size()==5&&child(pose.children.back(),"Node").properties[0].bytes==prop(std::int64_t{102}).bytes);
    check(child(pose.children.back(),"Matrix").properties[0].bytes==array(Matrix4{}.elements()).bytes);
    appendFbxMeshScene({"Empty",202,203,{},{}},objects,connections,pose);
    check(child(objects.children.back(),"Properties70").children.size()==5&&connections.children.size()==7&&pose.children.size()==6);
    edge(connections.children[5],202,203);edge(connections.children[6],203,0);
}
