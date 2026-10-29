#pragma once
#include "MeshPointMask.h"
#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <vector>

namespace iiActorObject::object3d_detail
{
struct Adjustment;
enum class TemplateSurfaceRemapMode {Change,Removal};
enum class TemplateSurfaceMaskKind {Layer,NamedSelection};
struct TemplateSurfaceMaskRemapResult final
{
    std::shared_ptr<const std::vector<MeshPointMask>> value;
    bool changed=false;
};
struct TemplateSurfaceAdjustmentRemapResult final
{
    std::shared_ptr<const Adjustment> value;
    // Non-null only for a detached copy, usable by the resource-slot stage.
    std::shared_ptr<Adjustment> writable;
};
// Borrows an unchanged mapping and removed set for its lifetime. It never edits
// input snapshots; unchanged values retain their original shared ownership.
class TemplateSurfaceEditRemapper final
{
  public:
    TemplateSurfaceEditRemapper(std::span<const std::optional<std::size_t>> mapping,
        const std::set<std::size_t> &removed,bool discardRemoved,TemplateSurfaceRemapMode mode);
    [[nodiscard]] TemplateSurfaceMaskRemapResult masks(
        const std::shared_ptr<const std::vector<MeshPointMask>> &source,TemplateSurfaceMaskKind kind) const;
    [[nodiscard]] TemplateSurfaceAdjustmentRemapResult adjustment(const std::shared_ptr<const Adjustment> &source) const;
  private:
    [[nodiscard]] std::size_t retained(std::size_t old) const;
    [[nodiscard]] bool changesRemovalIndex(std::size_t old) const;
    std::span<const std::optional<std::size_t>> mapping_;
    const std::set<std::size_t> &removed_;
    bool discardRemoved_;
    TemplateSurfaceRemapMode mode_;
};
} // namespace iiActorObject::object3d_detail
