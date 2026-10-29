#include "../src/Interchange/FBX/Object3DFbxSkeletonWrite.h"
#include "../src/Interchange/FBX/Object3DFbxSceneNodes.h"
#include <iiActorObject/Skeleton.h>
#include <array>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
using namespace iiActorObject::object3d_detail::fbx_binary;
namespace
{
void check(bool value)
{if(!value)throw std::runtime_error("FBX skeleton writer contract failed");}
template<class E,class F>void rejects(F action)
{try{action();}catch(const E &){return;}throw std::runtime_error("Invalid FBX skeleton accepted or wrong error");}
const Node &child(const Node &node,std::string_view name)
{for(const auto &entry:node.children)if(entry.name==name)return entry;throw std::runtime_error("Missing FBX node");}
void property(const Node &node,std::string_view name,std::string_view text)
{check(node.name=="P"&&node.properties[0].bytes==prop(name).bytes&&node.properties[4].bytes==prop(text).bytes);}
void edge(const Node &node,std::int64_t source,std::int64_t target)
{check(node.name=="C"&&node.properties.size()==3&&node.properties[0].bytes==prop("OO").bytes&&node.properties[1].bytes==prop(source).bytes&&node.properties[2].bytes==prop(target).bytes);}
std::size_t identityCalls=0;
Node boneIdentity(std::size_t index)
{++identityCalls;return fbx_scene_nodes::externalInteger("iiActorObjectSourceBone",fbx_scene_nodes::externalIndex(index));}
Node alternativeIdentity(std::size_t index)
{return fbx_scene_nodes::externalInteger("customBone",fbx_scene_nodes::externalIndex(index));}
Node failingIdentity(std::size_t index)
{if(index==1)throw std::runtime_error("identity provider failed");return boneIdentity(index);}
}
int main()
{
    std::vector<Bone> bones{{"child",2,{{1,0,0},{},{1,1,1}}},{"other",NoBone,{{0,1,0},{},{1,1,1}}},{"root",NoBone,{}}};
    std::array<FbxSkeletonSemantic,3> semantics{{{0,"hand"},{1,"face"},{0,"finger"}}};
    const std::array worlds{Transform{{1,0,0},{},{1,1,1}}.matrix(),Transform{{0,2,0},{},{1,1,1}}.matrix(),Matrix4{}};
    const std::array inverse{Transform{{-1,0,0},{},{1,1,1}}.matrix(),Transform{{0,-2,0},{},{1,1,1}}.matrix(),Matrix4{}};
    std::int64_t next=1000;Node objects{"Objects",{},{}},connections{"Connections",{},{}};
    const FbxSkeletonWriteInput input{bones,semantics,worlds,inverse,100,3,2,boneIdentity};
    auto result=writeFbxSkeleton(input,next,objects,connections);
    check(next==1007&&result.boneIds==std::vector<std::int64_t>({1001,1003,1005})&&identityCalls==6);
    check(result.skinBones.size()==3&&result.skinBones[0].name=="child"&&result.skinBones[0].nodeId==1001&&
        result.skinBones[0].bindWorld.elements()==worlds[0].elements()&&result.skinBones[0].inverseBind.elements()==inverse[0].elements());
    check(objects.children.size()==6&&connections.children.size()==6);
    edge(connections.children[0],1002,1001);edge(connections.children[1],1001,1005);
    edge(connections.children[2],1004,1003);edge(connections.children[3],1003,100);
    edge(connections.children[4],1006,1005);edge(connections.children[5],1005,100);
    const auto &fields=child(objects.children[0],"Properties70").children;
    check(fields.size()==8&&fields[5].properties[0].bytes==prop("iiActorObjectSourceBone").bytes&&fields[5].properties[4].bytes==prop(std::int32_t{0}).bytes);
    property(fields[6],"iiActorSemantic.0","hand");property(fields[7],"iiActorSemantic.1","finger");
    const auto &attrFields=child(objects.children[1],"Properties70").children;
    check(attrFields.size()==2&&attrFields[1].properties[0].bytes==prop("iiActorObjectSourceBone").bytes);
    check(result.pose.name=="Pose"&&result.pose.properties[0].bytes==prop(std::int64_t{1000}).bytes&&
        child(result.pose,"NbPoseNodes").properties[0].bytes==prop(std::int32_t{6}).bytes&&result.pose.children.size()==7);
    check(child(result.pose.children[4],"Node").properties[0].bytes==prop(std::int64_t{1001}).bytes&&
        child(result.pose.children[4],"Matrix").properties[0].bytes==array(worlds[0].elements()).bytes);
    appendFbxBindPoseNode(result.pose,200,Matrix4{});
    check(result.pose.children.size()==8&&child(result.pose.children.back(),"Node").properties[0].bytes==prop(std::int64_t{200}).bytes);

    auto withoutIdentity=input;withoutIdentity.boneIdentity=nullptr;
    std::int64_t plainNext=2000;Node plainObjects{"Objects",{},{}},plainConnections{"Connections",{},{}};
    (void)writeFbxSkeleton(withoutIdentity,plainNext,plainObjects,plainConnections);
    const auto &plainFields=child(plainObjects.children[0],"Properties70").children;
    check(plainFields.size()==7&&child(plainObjects.children[1],"Properties70").children.size()==1&&identityCalls==6);
    property(plainFields[5],"iiActorSemantic.0","hand");property(plainFields[6],"iiActorSemantic.1","finger");

    auto alternative=input;alternative.boneIdentity=alternativeIdentity;
    std::int64_t alternativeNext=3000;Node alternativeObjects{"Objects",{},{}},alternativeConnections{"Connections",{},{}};
    (void)writeFbxSkeleton(alternative,alternativeNext,alternativeObjects,alternativeConnections);
    const auto &alternativeFields=child(alternativeObjects.children[0],"Properties70").children;
    check(alternativeFields[5].properties[0].bytes==prop("customBone").bytes&&identityCalls==6);

    next=5;Node staticObjects{"Objects",{},{}},staticConnections{"Connections",{},{}};
    const FbxSkeletonWriteInput plain{{},{},{},{},8,1,2,nullptr};
    auto staticResult=writeFbxSkeleton(plain,next,staticObjects,staticConnections);
    check(next==6&&staticResult.boneIds.empty()&&staticResult.skinBones.empty()&&staticObjects.children.empty()&&staticConnections.children.empty());
    check(child(staticResult.pose,"NbPoseNodes").properties[0].bytes==prop(std::int32_t{4}).bytes&&staticResult.pose.children.size()==4);

    Node invalidObjects{"Objects",{},{}},invalidConnections{"Connections",{},{}};next=100;
    auto invalid=input;invalid.bindWorlds=std::span<const Matrix4>{worlds}.first(2);
    rejects<std::invalid_argument>([&]{(void)writeFbxSkeleton(invalid,next,invalidObjects,invalidConnections);});
    auto badBones=bones;badBones[0].parent=9;invalid=input;invalid.bones=badBones;
    rejects<std::invalid_argument>([&]{(void)writeFbxSkeleton(invalid,next,invalidObjects,invalidConnections);});
    badBones=bones;badBones[2].parent=0;invalid.bones=badBones;
    rejects<std::invalid_argument>([&]{(void)writeFbxSkeleton(invalid,next,invalidObjects,invalidConnections);});
    const std::array badSemantics{FbxSkeletonSemantic{9,"invalid"}};
    invalid=input;invalid.semantics=badSemantics;
    rejects<std::invalid_argument>([&]{(void)writeFbxSkeleton(invalid,next,invalidObjects,invalidConnections);});
    invalid=input;invalid.sourceBoneCount=2;
    rejects<std::invalid_argument>([&]{(void)writeFbxSkeleton(invalid,next,invalidObjects,invalidConnections);});
    invalid=input;invalid.meshCount=static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
    rejects<std::length_error>([&]{(void)writeFbxSkeleton(invalid,next,invalidObjects,invalidConnections);});
    next=-1;
    rejects<std::overflow_error>([&]{(void)writeFbxSkeleton(input,next,invalidObjects,invalidConnections);});
    next=std::numeric_limits<std::int64_t>::max();
    rejects<std::overflow_error>([&]{(void)writeFbxSkeleton(input,next,invalidObjects,invalidConnections);});
    check(next==std::numeric_limits<std::int64_t>::max()&&invalidObjects.children.empty()&&invalidConnections.children.empty());

    auto providerFailure=input;providerFailure.boneIdentity=failingIdentity;
    std::int64_t failureNext=500;Node failureObjects{"Objects",{},{}},failureConnections{"Connections",{},{}};
    rejects<std::runtime_error>([&]{(void)writeFbxSkeleton(providerFailure,failureNext,failureObjects,failureConnections);});
    check(failureNext==507&&failureObjects.children.size()==2&&failureConnections.children.size()==2&&bones[0].name=="child");
}
