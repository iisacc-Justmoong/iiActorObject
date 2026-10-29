#include "Object3DTemplateSurfaceSelectionRemap.h"
#include "Object3DSelectionData.h"
#include "Object3DTemplateSurfaceEditRemap.h"
#include <utility>

namespace iiActorObject::object3d_detail
{
TemplateSurfaceSelectionRemapResult remapTemplateSurfaceSelections(
    const std::shared_ptr<const PointSelectionState> &source,const TemplateSurfaceEditRemapper &remapper)
{
    if(!source)return {source,false};
    std::shared_ptr<PointSelectionState> copy;
    for(std::size_t i=0;i<source->entries.size();++i)if(source->entries[i].masks)
    {
        auto masks=remapper.masks(source->entries[i].masks,TemplateSurfaceMaskKind::NamedSelection);
        if(masks.changed)
        {
            if(!copy)copy=std::make_shared<PointSelectionState>(*source);
            copy->entries[i].masks=std::move(masks.value);
        }
    }
    if(!copy)return {source,false};
    return {std::move(copy),true};
}
} // namespace iiActorObject::object3d_detail
