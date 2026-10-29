#pragma once
#include "Object3DTemplateTypes.h"
#include "SurfaceBindingCore.h"
#include "SurfaceFitSettings.h"
#include <cstddef>
#include <optional>
#include <set>
#include <span>
#include <vector>

namespace iiActorObject
{
struct Object3DMesh;
namespace object3d_detail
{
// Values relevant to changing surface slots; engine handles and editor history
// remain with the caller. Operations consume their input value on an rvalue call.
struct TemplateSurfaceReferences final
{
    std::vector<TemplateTarget> targets;
    std::vector<TemplateJointBinding> joints;
    std::optional<TemplateGrounding> grounding;
    std::vector<SurfaceBinding> surfaces;
    std::vector<std::optional<SurfaceBindingBasis>> bases;
    std::vector<SurfaceFittingRule> fittings;
};
[[nodiscard]] std::size_t templateSurfaceBindingIndex(std::span<const SurfaceBinding> surfaces,std::size_t target);
[[nodiscard]] std::optional<SurfaceBindingBasis> templateSurfaceBindingBasis(
    std::span<const std::optional<SurfaceBindingBasis>> bases,std::size_t index);
void validateAddedTemplateSurfaceReferences(std::span<const TemplateTarget> targets,
    std::span<const TemplateJointBinding> joints,const TemplateGrounding *grounding,const std::set<std::size_t> &added);
void validateReplacementTemplateSurfaceReferences(std::span<const TemplateTarget> targets,
    std::span<const TemplateJointBinding> joints,const TemplateGrounding *grounding,
    std::span<const SurfaceBinding> surfaces,const std::set<std::size_t> &replaced);
void validateReplacementTemplateSurfaceFittings(std::span<const SurfaceFittingRule> fittings,const std::set<std::size_t> &replaced);
// Borrowed values must remain unchanged and must not alias a consumed source.
[[nodiscard]] TemplateSurfaceReferences remapTemplateSurfaceReferences(TemplateSurfaceReferences source,
    std::span<const std::optional<std::size_t>> oldToNew,const std::set<std::size_t> &removed,
    const std::set<std::size_t> &visibility,std::span<const Object3DMesh> before,std::span<const Object3DMesh> after,
    std::span<const SurfaceFittingRule> incoming,const std::set<std::size_t> &added);
[[nodiscard]] std::vector<SurfaceBinding> remapReplacementTemplateVisibility(std::vector<SurfaceBinding> surfaces,
    const std::set<std::size_t> &replaced,const std::set<std::size_t> &visibility,
    std::span<const Object3DMesh> before,std::span<const Object3DMesh> after);
} // namespace object3d_detail
} // namespace iiActorObject
