#pragma once
#include <memory>

namespace iiActorObject::object3d_detail
{
struct PointSelectionState;
class TemplateSurfaceEditRemapper;
struct TemplateSurfaceSelectionRemapResult final
{
    std::shared_ptr<const PointSelectionState> value;
    bool changed=false;
};
// Inputs are borrowed during the call and never changed. Unchanged selections
// retain their original shared state; changed masks are copied on demand.
[[nodiscard]] TemplateSurfaceSelectionRemapResult remapTemplateSurfaceSelections(
    const std::shared_ptr<const PointSelectionState> &source,const TemplateSurfaceEditRemapper &remapper);
} // namespace iiActorObject::object3d_detail
