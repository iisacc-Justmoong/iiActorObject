#include "Object3DTemplateSurfaceFollowerNative.h"
#include "SurfaceAttachment.h"

namespace iiActorObject::object3d_detail
{
void TemplateSurfaceFollowerNativeOperations::fit(Object3DData &data,const SurfaceBinding &binding,
    const std::optional<SurfaceBindingBasis> &basis) const
{
    if(basis)fitMeshSurfaceBindingWithBasis(data,binding,*basis);else fitMeshSurfaceBinding(data,binding);
}
void TemplateSurfaceFollowerNativeOperations::transfer(Object3DData &data,const SurfaceBinding &binding,
    const std::optional<SurfaceBindingBasis> &basis) const
{
    if(basis)(void)transferMeshBindingWeightsWithBasis(data,binding,*basis,0,NoBone,SkinningProfileTransfer::Source);
    else (void)transferMeshBindingWeights(data,binding,0,NoBone,SkinningProfileTransfer::Source);
}
} // namespace iiActorObject::object3d_detail
