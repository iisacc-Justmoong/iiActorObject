#include "Object3DFbxSkeleton.h"
#include "CharacterMath.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace iiActorObject::object3d_detail
{
using namespace character_detail;
using fbx_fields::externalIndex;
using fbx_fields::property;
using fbx_fields::string;
using fbx_fields::unique;
namespace
{
bool same(const ufbx_matrix &a, const ufbx_matrix &b)
{
    for (unsigned i = 0; i < 12; ++i)
        if (std::abs(a.v[i] - b.v[i]) > 1e-7 * std::max({1.0, std::abs(a.v[i]), std::abs(b.v[i])}))
            return false;
    return true;
}
Transform transform(const ufbx_matrix &m,double tolerance,double &error)
{
    const auto t = ufbx_matrix_to_transform(&m);
    Transform result{
        {t.translation.x,t.translation.y,t.translation.z},
        {t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w}, {t.scale.x,t.scale.y,t.scale.z}};
    result = skeletalTransform(result);
    const auto check = result.matrix().elements();
    for (unsigned col = 0; col < 4; ++col)
        for (unsigned row = 0; row < 3; ++row)
        {
            const auto difference=std::abs(check[col*4+row]-m.v[col*3+row])/std::max(1.0,std::abs(m.v[col*3+row]));
            if(!std::isfinite(difference)||difference>tolerance)
                throw std::invalid_argument(
                    "FBX joint shear requires baking into an SRT skeleton before import");
            error=std::max(error,difference);
        }
    return result;
}
} // namespace
FbxSkeletonData readFbxSkeleton(const ufbx_scene &scene,const FbxSkeletonReadOptions &options,const FbxWarningSink &warn)
{
    if(options.maxBones==0||options.maxBones>65535||!std::isfinite(options.maximumJointMatrixError)||
       options.maximumJointMatrixError<0||options.maximumJointMatrixError>1e-3)
        throw std::invalid_argument("Invalid FBX skeleton limits");
    if(options.kind!=Object3DKind::Automatic&&options.kind!=Object3DKind::StaticMesh&&options.kind!=Object3DKind::SkeletalMesh)
        throw std::invalid_argument("Invalid FBX object kind");
    FbxSkeletonData result;
    result.kind=options.kind;
    std::unordered_set<const ufbx_node *> included;
    for(const auto *node:scene.nodes)if(node->bone)included.insert(node);
    std::map<const ufbx_node *, ufbx_matrix> bindWorld;
    for (const auto *skin : scene.skin_deformers)
    {
        if (skin->skinning_method != UFBX_SKINNING_METHOD_LINEAR &&
            skin->skinning_method != UFBX_SKINNING_METHOD_DUAL_QUATERNION &&
            skin->skinning_method != UFBX_SKINNING_METHOD_BLENDED_DQ_LINEAR)
            throw std::invalid_argument("Unsupported FBX skinning method");
        for (const auto *cluster : skin->clusters)
        {
            included.insert(cluster->bone_node);
            auto [it, added] = bindWorld.emplace(cluster->bone_node, cluster->bind_to_world);
            if (!added && !same(it->second, cluster->bind_to_world))
                throw std::invalid_argument("Inconsistent FBX bind matrices for one bone");
        }
    }
    if (included.size() > options.maxBones)
        throw std::invalid_argument("FBX skeleton exceeds import limit");
    if (included.empty())
    {
        if (result.kind == Object3DKind::SkeletalMesh)
            throw std::invalid_argument("Skeletal FBX has no skeleton");
        result.kind = Object3DKind::StaticMesh;
    }
    const auto parentOf = [&](const ufbx_node *n)
    {
        auto *p = n->parent;
        std::size_t steps = 0;
        while (p && !included.contains(p))
        {
            if (++steps > scene.nodes.count)
                throw std::invalid_argument("FBX parent cycle");
            p = p->parent;
        }
        return p;
    };
    auto &ordered=result.ordered;
    std::unordered_set<const ufbx_node *> emitted;
    while (ordered.size() < included.size())
    {
        bool progress = false;
        for (const auto *node : scene.nodes)
            if (included.contains(node) && !emitted.contains(node))
                if (const auto *parent = parentOf(node); !parent || emitted.contains(parent))
                {
                    ordered.push_back(node);
                    emitted.insert(node);
                    progress = true;
                }
        if (!progress)
            throw std::invalid_argument("FBX bone hierarchy is cyclic");
    }
    std::size_t roots = 0;
    for (auto *n : ordered)
        if (!parentOf(n))
            ++roots;
    std::set<std::string> boneNames;
    auto &boneIndex=result.boneIndex;
    std::unordered_map<const ufbx_node *, ufbx_matrix> restWorld;
    if (roots > 1)
    {
        if (included.size() + 1 > options.maxBones)
            throw std::invalid_argument("FBX synthetic root exceeds joint limit");
        for (auto *n : ordered)
            boneNames.insert(string(n->name));
        const auto rootName = unique("iiImportedRoot", boneNames);
        boneNames.clear();
        boneNames.insert(rootName);
        result.bones.push_back({rootName, NoBone, {}});
    }
    std::set<std::string> semanticNames;std::string errorJoint;
    for (const auto *node : ordered)
    {
        const auto *parent = parentOf(node);
        result.parents.emplace(node,parent);
        auto world = node->node_to_world;
        if (const auto found = bindWorld.find(node); found != bindWorld.end())
            world = found->second;
        else if (parent)
        {
            const auto inverse = ufbx_matrix_invert(&parent->node_to_world);
            const auto relative = ufbx_matrix_mul(&inverse, &node->node_to_world);
            world = ufbx_matrix_mul(&restWorld.at(parent), &relative);
        }
        restWorld.emplace(node, world);
        auto local = world;
        if (parent)
        {
            const auto inverse = ufbx_matrix_invert(&restWorld.at(parent));
            local = ufbx_matrix_mul(&inverse, &world);
        }
        const auto index = result.bones.size();
        boneIndex.emplace(node, index);
        const auto original = string(node->name), name = unique(original, boneNames);
        if (name != original)
            warn("Empty or duplicate bone names were assigned unique names.");
        double reconstructionError=0;const auto localTransform=transform(local,options.maximumJointMatrixError,reconstructionError);
        if(reconstructionError>result.maximumJointMatrixError){result.maximumJointMatrixError=reconstructionError;errorJoint=name;}
        result.bones.push_back({name,
                              parent      ? boneIndex.at(parent)
                              : roots > 1 ? BoneIndex{0}
                                          : NoBone,
                              localTransform});
        if(options.includeExternalIds)
        {
            const auto *modelId=property(node->props,"iiActorObjectSourceBone"),*attributeId=node->bone?property(node->bone->props,"iiActorObjectSourceBone"):nullptr;
            if(modelId&&attributeId&&externalIndex(modelId)!=externalIndex(attributeId))throw std::invalid_argument("Conflicting external joint IDs");
            if(modelId||attributeId)result.externalBoneIds.emplace_back(index,externalIndex(modelId?modelId:attributeId));
        }
        bool hasSemantic = false;
        for (const auto &p : node->props.props)
            if (std::string_view(p.name.data, p.name.length).starts_with("iiActorSemantic."))
            {
                auto label = string(p.value_str);
                if (!semanticNames.insert(label).second)
                    throw std::invalid_argument("Duplicate FBX joint semantic");
                result.semantics.push_back({std::move(label), index});
                hasSemantic = true;
            }
        if (!hasSemantic && semanticNames.insert(name).second)
            result.semantics.push_back({name, index});
    }
    if(result.maximumJointMatrixError>1e-7)
    {
        std::ostringstream message;message.imbue(std::locale::classic());message<<"FBX joint transforms were reconstructed within the configured tolerance; maximum matrix error "<<std::setprecision(8)<<result.maximumJointMatrixError<<" at "<<errorJoint;warn(message.str());
    }
    return result;
}
} // namespace iiActorObject::object3d_detail
