#pragma once
// Private engine-owned registries; references remain non-owning.
#include "Object3DEngine.h"
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace iiActorObject
{
namespace object3d_detail
{
struct AssetReference final
{
    Object3DId id;
    std::weak_ptr<State> owner;
    std::weak_ptr<const Object3DAsset> asset;
};
struct EditorReference final
{
    Object3DId id;
    std::weak_ptr<State> owner;
    std::weak_ptr<Object3DEditor> editor;
};
struct RigReference final
{
    Object3DId id;
    std::weak_ptr<State> owner;
    std::weak_ptr<Object3DRigSession> rig;
};
struct TemplateReference final
{
    Object3DId id;
    std::weak_ptr<State> owner;
    std::weak_ptr<const Object3DTemplate> program;
};
struct State final
{
    struct AssetEntry final
    {
        std::shared_ptr<const Object3DAsset> asset;
        std::weak_ptr<const AssetReference> reference;
    };
    struct EditorEntry final
    {
        std::shared_ptr<Object3DEditor> editor;
        std::weak_ptr<const EditorReference> reference;
    };
    struct RigEntry final
    {
        std::shared_ptr<Object3DRigSession> rig;
        std::weak_ptr<const RigReference> reference;
    };
    struct TemplateEntry final
    {
        std::shared_ptr<const Object3DTemplate> program;
        std::weak_ptr<const TemplateReference> reference;
    };
    Object3DEngineLimits limits;
    Object3DId nextObject = 1, nextEditor = 1;
    Object3DId nextRig = 1;
    Object3DId nextTemplate = 1;
    std::unordered_map<Object3DId, AssetEntry> assets;
    std::unordered_map<Object3DId, EditorEntry> editors;
    std::unordered_map<Object3DId, RigEntry> rigs;
    std::unordered_map<Object3DId, TemplateEntry> templates;
};
} // namespace object3d_detail
namespace
{
inline auto checked(const std::weak_ptr<object3d_detail::State> &reference)
{
    auto state = reference.lock();
    if (!state)
        throw std::logic_error("The owning 3D engine has expired");
    return state;
}
inline std::size_t collect(object3d_detail::State &state)
{
    // Drop sessions first so their history references can release asset tokens.
    auto count = std::erase_if(state.editors, [](const auto &v) { return v.second.reference.expired(); });
    count += std::erase_if(state.rigs, [](const auto &v) { return v.second.reference.expired(); });
    count += std::erase_if(state.templates, [](const auto &v) { return v.second.reference.expired(); });
    count += std::erase_if(state.assets, [](const auto &v) { return v.second.reference.expired(); });
    return count;
}
inline void capacity(Object3DId next, std::size_t size, std::size_t limit)
{
    if (next == std::numeric_limits<Object3DId>::max())
        throw std::overflow_error("3D resource identity exhausted");
    if (size >= limit)
        throw std::length_error("3D engine resource limit reached");
}
} // namespace
} // namespace iiActorObject
