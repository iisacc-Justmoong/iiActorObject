#include "../src/Interchange/FBX/Object3DFbxSceneMetadataWrite.h"
#include "../src/Interchange/FBX/Object3DFbxSceneNodes.h"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace iiActorObject::object3d_detail;
using namespace iiActorObject::object3d_detail::fbx_binary;
namespace
{
void check(bool value)
{if(!value)throw std::runtime_error("FBX scene metadata writer contract failed");}
void field(const Node &node,std::string_view key,std::string_view value)
{check(node.name=="P"&&node.properties.size()==5&&node.properties[0].bytes==prop(key).bytes&&
    node.properties[1].bytes==prop("KString").bytes&&node.properties[2].bytes==prop("").bytes&&
    node.properties[3].bytes==prop("U").bytes&&node.properties[4].bytes==prop(value).bytes);}
}
int main()
{
    std::vector<Node> properties{{"Existing",{},{}}},children{{"Child",{},{}}};
    appendFbxSceneMetadata(properties,children,"Scene",2,std::nullopt);
    check(properties.size()==3&&children.size()==1&&properties[0].name=="Existing");
    field(properties[1],"iiObject3DName","Scene");
    check(properties[2].properties[0].bytes==prop("iiObject3DKind").bytes&&properties[2].properties[1].bytes==prop("int").bytes&&
        properties[2].properties[2].bytes==prop("Integer").bytes&&properties[2].properties[3].bytes==prop("").bytes&&properties[2].properties[4].bytes==prop(std::int32_t{2}).bytes);
    field(fbxBoneSemanticProperty(2,"left.hand"),"iiActorSemantic.2","left.hand");
    std::array<std::uint8_t,4> catalog{0,1,0xff,7};
    appendFbxSceneMetadata(properties,children,"Second",1,std::span<const std::uint8_t>{catalog});
    check(children.size()==2&&children[1].name=="iiActorObjectRigSamples"&&children[1].children.size()==2);
    check(children[1].children[0].name=="Version"&&children[1].children[0].properties[0].bytes==prop(1).bytes);
    check(children[1].children[1].name=="Data"&&children[1].children[1].properties[0].bytes==blob(catalog).bytes);
    appendFbxSceneMetadata(properties,children,"Empty",1,std::span<const std::uint8_t>{});
    check(children.size()==3&&children[2].children[1].properties[0].bytes==blob(std::span<const std::uint8_t>{}).bytes);
    auto model=fbx_scene_nodes::model(4,"Root","Null");
    appendFbxSceneMetadata(model.children[1].children,model.children,"Nested",2,std::span<const std::uint8_t>{catalog});
    field(model.children[1].children[5],"iiObject3DName","Nested");
    check(model.children.back().name=="iiActorObjectRigSamples");
}
