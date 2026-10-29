#pragma once
#include <cstddef>
#include <optional>
#include <set>
#include <span>
#include <vector>

namespace iiActorObject
{
struct Bone;
namespace object3d_detail
{
enum class TemplateSurfaceOperation {Change,Remove,Replace};
struct TemplateSurfaceSlotPlan final
{
    // Source slots are removed for Change/Remove and replaced for Replace.
    std::set<std::size_t> sourceSlots,addedSlots,visibility;
    // Replace keeps slot identities and does not need either mapping.
    std::vector<std::optional<std::size_t>> oldToNew,newToOld;
};
// Validates slot selections and skeleton identities without owning an editor or asset.
// Returned mappings own their values; input spans are only borrowed during the call.
[[nodiscard]] TemplateSurfaceSlotPlan planTemplateSurfaceSlots(TemplateSurfaceOperation operation,
    std::size_t oldCount,std::size_t newCount,std::span<const Bone> oldBones,std::span<const Bone> newBones,
    std::span<const std::size_t> sourceSlots,std::span<const std::size_t> addedSlots,
    std::span<const std::size_t> visibilitySlots);
} // namespace object3d_detail
} // namespace iiActorObject
