#include "../src/Interchange/FBX/Object3DFbxSkeleton.h"
#include <array>
#include <limits>
#include <string_view>
#include <stdexcept>
#include <string>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool condition)
{if(!condition)throw std::runtime_error("FBX skeleton reader contract failed");}
template<class F> void rejects(F operation)
{try{operation();}catch(const std::exception &){return;}throw std::runtime_error("Invalid FBX skeleton was accepted");}
}
int main()
{
    std::vector<std::string> warnings;
    const FbxWarningSink warn=[&](std::string text){warnings.push_back(std::move(text));};
    FbxSkeletonReadOptions options{8,1e-7,Object3DKind::Automatic,false};
    ufbx_scene scene{};
    auto empty=readFbxSkeleton(scene,options,warn);
    check(empty.kind==Object3DKind::StaticMesh&&empty.bones.empty());
    options.kind=Object3DKind::SkeletalMesh;
    rejects([&]{(void)readFbxSkeleton(scene,options,warn);});
    options.kind=Object3DKind::Automatic;
    options.maxBones=0;
    rejects([&]{(void)readFbxSkeleton(scene,options,warn);});
    options.maxBones=8;options.maximumJointMatrixError=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{(void)readFbxSkeleton(scene,options,warn);});
    options.maximumJointMatrixError=1e-7;

    ufbx_bone rootBone{},childBone{};ufbx_node root{},child{};
    root.name={"root",4};root.bone=&rootBone;root.node_to_world=ufbx_identity_matrix;
    child.name={"child",5};child.bone=&childBone;child.parent=&root;child.node_to_world=ufbx_identity_matrix;
    std::array<ufbx_node *,2> nodes{&child,&root};scene.nodes={nodes.data(),nodes.size()};
    auto skeleton=readFbxSkeleton(scene,options,warn);
    check(skeleton.bones.size()==2&&skeleton.bones[0].name=="root"&&skeleton.bones[1].name=="child");
    check(skeleton.bones[0].parent==NoBone&&skeleton.bones[1].parent==0);
    check(skeleton.boneIndex.at(&root)==0&&skeleton.boneIndex.at(&child)==1);
    check(skeleton.parents.at(&child)==&root&&skeleton.parents.at(&root)==nullptr);
    check(skeleton.semantics.size()==2&&skeleton.maximumJointMatrixError<=1e-7);
    constexpr std::string_view key="iiActorObjectSourceBone";
    ufbx_prop modelId{},attributeId{};
    modelId.name={key.data(),key.size()};modelId.type=UFBX_PROP_INTEGER;modelId.value_int=7;
    attributeId=modelId;root.props.props={&modelId,1};rootBone.props.props={&attributeId,1};
    options.includeExternalIds=true;
    const auto identified=readFbxSkeleton(scene,options,warn);
    check(identified.externalBoneIds.size()==1&&identified.externalBoneIds[0].first==0&&identified.externalBoneIds[0].second==7);
    attributeId.value_int=8;
    rejects([&]{(void)readFbxSkeleton(scene,options,warn);});
    options.includeExternalIds=false;root.props.props={};rootBone.props.props={};

    child.parent=nullptr;
    auto multiple=readFbxSkeleton(scene,options,warn);
    check(multiple.bones.size()==3&&multiple.bones[0].name=="iiImportedRoot");
    check(multiple.bones[1].parent==0&&multiple.bones[2].parent==0);
    options.maxBones=2;
    rejects([&]{(void)readFbxSkeleton(scene,options,warn);});
    options.maxBones=8;child.parent=&root;root.parent=&child;
    rejects([&]{(void)readFbxSkeleton(scene,options,warn);});
    root.parent=nullptr;root.node_to_world.v[3]=.2;
    rejects([&]{(void)readFbxSkeleton(scene,options,warn);});
}
