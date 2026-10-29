#pragma once
#include <cstddef>

namespace iiActorObject::object3d_detail::glb_import
{
// Per-import work counters; owners check configured limits before advancing them.
struct GeometryBudgets final
{
    std::size_t vertices=0,triangles=0,primitives=0,morphValues=0,keyBytes=0,tangents=0,tangentDeltas=0,flatLayouts=0;
    std::size_t identityPoints=0,identityMorphs=0,flatMorphWork=0;
    std::size_t tangentNoteWarnings=0;bool tangentNotesTruncated=false;
};
}
