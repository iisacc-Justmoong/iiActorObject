#include "../src/Interchange/FBX/Object3DFbxPoseWrite.h"
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>
using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
using namespace iiActorObject::object3d_detail::fbx_binary;
namespace
{
void check(bool value){if(!value)throw std::runtime_error("FBX pose writer contract failed");}
template<class E,class F>void rejects(F action)
{try{action();}catch(const E &){return;}throw std::runtime_error("Invalid FBX pose accepted or wrong error");}
const Node &child(const Node &node,std::string_view name)
{for(const auto &entry:node.children)if(entry.name==name)return entry;throw std::runtime_error("Missing FBX pose node");}
}
int main()
{
    auto pose=makeFbxBindPose(42,3,2);
    check(pose.name=="Pose"&&pose.properties[0].bytes==prop(std::int64_t{42}).bytes&&pose.children.size()==3);
    check(child(pose,"NbPoseNodes").properties[0].bytes==prop(std::int32_t{6}).bytes);
    const auto transform=Transform{{1,2,3},{},{1,1,1}}.matrix();
    appendFbxBindPoseNode(pose,91,transform);
    check(pose.children.size()==4&&child(pose.children.back(),"Node").properties[0].bytes==prop(std::int64_t{91}).bytes);
    check(child(pose.children.back(),"Matrix").properties[0].bytes==array(transform.elements()).bytes);
    auto empty=makeFbxBindPose(3,0,0);check(child(empty,"NbPoseNodes").properties[0].bytes==prop(std::int32_t{1}).bytes);
    rejects<std::length_error>([]{(void)makeFbxBindPose(1,std::numeric_limits<std::size_t>::max(),0);});
    rejects<std::length_error>([]{(void)makeFbxBindPose(1,0,std::numeric_limits<std::size_t>::max());});
    rejects<std::length_error>([]{(void)makeFbxBindPose(1,static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())-1,1);});
}
