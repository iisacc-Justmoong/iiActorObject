#pragma once
#include <iiActorObject/Animation.h>
#include <cstddef>
#include <span>
#include <string_view>

namespace iiActorObject
{
struct GlbAnimationExportOptions final
{
    bool allowDiscardScalarCurves = false;
    std::size_t maximumKeys = 1000000;
};
struct GlbMorphCurveBinding final
{
    std::string_view curve;
    std::string_view morph;
    // Empty targets every matching mesh; otherwise select this exact mesh name.
    std::string_view mesh{};
};
struct GlbMorphAnimationOptions final
{
    // Views are borrowed for this call. A curve can drive multiple targets;
    // each morph on a particular mesh can be driven by only one curve.
    std::span<const GlbMorphCurveBinding> bindings;
    bool matchUnmappedCurvesByName = false;
    std::size_t maximumWeightValues = 16'000'000;
    std::size_t maximumCurveKeyVisits = 16'000'000;
    std::size_t maximumMappingEntries = 1'000'000;
    // Curve, mesh and logical morph indices; borrowed during export.
    std::span<const AnimationMorphBinding> indexedBindings;
};
}
