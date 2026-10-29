#include "Object3DFbxSkeletonWrite.h"
#include "Object3DFbxSceneMetadataWrite.h"
#include "Object3DFbxSceneNodes.h"
#include <iiActorObject/Skeleton.h>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace iiActorObject::object3d_detail
{
using namespace fbx_binary;
using namespace fbx_scene_nodes;

FbxSkeletonWriteResult writeFbxSkeleton(const FbxSkeletonWriteInput &input,
    std::int64_t &next,Node &objects,Node &connections)
{
    const auto count=input.bones.size();
    if(input.sourceBoneCount<count||input.bindWorlds.size()<count||input.inverseBinds.size()<count)
        throw std::invalid_argument("Inconsistent FBX skeleton inputs");
    constexpr auto maxId=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if(count>(maxId-1)/2)throw std::overflow_error("FBX skeleton ID range");
    const auto required=std::uint64_t{1}+2*static_cast<std::uint64_t>(count);
    if(next<0||static_cast<std::uint64_t>(next)>maxId-required)
        throw std::overflow_error("FBX skeleton ID range");
    for(const auto &semantic:input.semantics)
        if(semantic.bone>=count)throw std::invalid_argument("Invalid FBX skeleton semantic index");
    for(const auto &bone:input.bones)
        if(bone.parent!=NoBone&&bone.parent>=count)throw std::invalid_argument("Invalid FBX skeleton parent");
    std::vector<std::uint8_t> marks(count);
    for(std::size_t start=0;start<count;++start)
    {
        auto current=start;
        while(current!=NoBone&&marks[current]==0){marks[current]=1;current=input.bones[current].parent;}
        if(current!=NoBone&&marks[current]==1)throw std::invalid_argument("Cyclic FBX skeleton");
        current=start;
        while(current!=NoBone&&marks[current]==1){marks[current]=2;current=input.bones[current].parent;}
    }

    FbxSkeletonWriteResult result;
    result.pose=makeFbxBindPose(next,input.sourceBoneCount,input.meshCount);
    ++next;
    appendFbxBindPoseNode(result.pose,input.rootId,Matrix4{});
    result.boneIds.reserve(count);result.skinBones.reserve(count);
    for(std::size_t i=0;i<count;++i)
    {
        const auto id=next;next+=2;result.boneIds.push_back(id);
        result.skinBones.push_back({input.bones[i].name,id,input.inverseBinds[i],input.bindWorlds[i]});
    }
    const auto connect=[&](std::int64_t child,std::int64_t parent)
    {connections.children.push_back({"C",{prop("OO"),prop(child),prop(parent)}, {}});};
    for(std::size_t i=0;i<count;++i)
    {
        const auto &bone=input.bones[i];const auto id=result.boneIds[i],attribute=id+1;
        auto boneModel=model(id,bone.name,"LimbNode",bone.bindTransform);
        if(input.boneIdentity)boneModel.children[1].children.push_back(input.boneIdentity(i));
        std::size_t semanticIndex=0;
        for(const auto &semantic:input.semantics)
            if(semantic.bone==i)boneModel.children[1].children.push_back(fbxBoneSemanticProperty(semanticIndex++,semantic.name));
        objects.children.push_back(std::move(boneModel));
        objects.children.push_back({"NodeAttribute",{prop(attribute),prop(objectName(bone.name,"NodeAttribute")),prop("LimbNode")},
            {value("TypeFlags",prop("Skeleton")),{"Properties70",{},{pNumber("Size",.01)}}}});
        if(input.boneIdentity)objects.children.back().children[1].children.push_back(input.boneIdentity(i));
        connect(attribute,id);
        connect(id,bone.parent==NoBone?input.rootId:result.boneIds[bone.parent]);
        appendFbxBindPoseNode(result.pose,id,input.bindWorlds[i]);
    }
    return result;
}
}
