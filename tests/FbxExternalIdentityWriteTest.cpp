#include "../src/Interchange/FBX/Object3DFbxExternalIdentityWrite.h"
#include "../src/Core/Object3DExternalIdentity.h"
#include "../src/Geometry/Object3DMesh.h"
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
using namespace iiActorObject::object3d_detail::fbx_binary;
namespace
{
void check(bool value)
{if(!value)throw std::runtime_error("FBX external identity writer contract failed");}
template<class E,class F>void rejects(F action)
{try{action();}catch(const E &){return;}throw std::runtime_error("Invalid FBX external identity input accepted or wrong error");}
void textProperty(const Node &node,std::string_view name,std::string_view text)
{check(node.name=="P"&&node.properties.size()==5&&node.properties[0].bytes==prop(name).bytes&&
    node.properties[1].bytes==prop("KString").bytes&&node.properties[2].bytes==prop("").bytes&&
    node.properties[3].bytes==prop("U").bytes&&node.properties[4].bytes==prop(text).bytes);}
}
int main()
{
    textProperty(fbxExternalDocumentProperty("doc/1"),"iiActorObjectDocument","doc/1");
    const auto bone=fbxExternalBoneProperty(17);
    check(bone.properties[0].bytes==prop("iiActorObjectSourceBone").bytes&&bone.properties[1].bytes==prop("int").bytes&&
        bone.properties[2].bytes==prop("Integer").bytes&&bone.properties[3].bytes==prop("U").bytes&&bone.properties[4].bytes==prop(std::int32_t{17}).bytes);
    rejects<std::length_error>([]{(void)fbxExternalBoneProperty(std::numeric_limits<std::size_t>::max());});
    ExternalMeshIdentity identity;identity.mesh=17;identity.source=42;
    identity.points={8,3,7,9,10};identity.polygons={4,2};identity.corners={8,7,4,1,3,2};
    identity.morphs={{0,"Smile"},{1,"Wide Eye"}};
    const auto plain=fbxExternalMeshProperties("doc/1",3,identity,nullptr);
    check(plain.size()==6);
    textProperty(plain[0],"iiActorObjectDocument","doc/1");
    check(plain[1].properties[0].bytes==prop("iiActorObjectSourceMesh").bytes&&plain[1].properties[4].bytes==prop(std::int32_t{3}).bytes);
    textProperty(plain[2],"iiActorObjectSourcePointIds","8,3,7,9,10");
    textProperty(plain[3],"iiActorObjectSourcePolygonIds","4,2");
    textProperty(plain[4],"iiActorObjectSourceCornerLayouts","8,7,4,1,3,2");
    textProperty(plain[5],"iiActorObjectSourceMorphs","iiActorObjectMorphNames 1 2\n\"Smile\" \"Smile\"\n\"Wide Eye\" \"Wide Eye\"\n");
    auto escaped=identity;escaped.morphs={{0,"Eye\"\\Mark"}};
    const auto escapedFields=fbxExternalMeshProperties("doc/1",3,escaped,nullptr);
    textProperty(escapedFields[5],"iiActorObjectSourceMorphs","iiActorObjectMorphNames 1 1\n\"Eye\\\"\\\\Mark\" \"Eye\\\"\\\\Mark\"\n");
    rejects<std::length_error>([&]{(void)fbxExternalMeshProperties("doc/1",std::numeric_limits<std::size_t>::max(),identity,nullptr);});

    MeshTopology source;
    source.points={{{0,0,0},{}},{{1,0,0},{}},{{1,1,0},{}},{{0,1,0},{}},{{2,1,0},{}}};
    MeshTopologyDefinition layout;layout.positionCount=5;
    layout.layouts={{0,NoTopologyIndex,NoTopologyIndex},{1,NoTopologyIndex,NoTopologyIndex},
        {2,NoTopologyIndex,NoTopologyIndex},{3,NoTopologyIndex,NoTopologyIndex},{4,NoTopologyIndex,NoTopologyIndex}};
    layout.polygons={{0,1,2},{0,2,3},{1,4,2}};layout.hiddenPolygons={1};layout.reverseWinding=true;
    source.layout=MeshTopologyLayout::create(layout,source.points);
    const auto authored=fbxExternalMeshProperties("doc/1",3,identity,&source);
    textProperty(authored[3],"iiActorObjectSourcePolygonIds","0,2");
    textProperty(authored[4],"iiActorObjectSourceCornerLayouts","2,1,0,2,4,1");
    source.layout.reset();
    rejects<std::invalid_argument>([&]{(void)fbxExternalMeshProperties("doc/1",3,identity,&source);});
}
