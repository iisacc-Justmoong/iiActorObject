#include "Object3DTemplateSurfaceMaterialNative.h"
#include "Object3DTemplateSurfaceResources.h"

namespace iiActorObject::object3d_detail
{
void TemplateSurfaceMaterialNative::linkMaterial(std::size_t candidate,std::size_t existing)
{resources_.linkMaterial(candidate,existing);}
void TemplateSurfaceMaterialNative::linkRetainedImages()
{resources_.linkRetainedImages();}
std::size_t TemplateSurfaceMaterialNative::material(std::size_t candidate)
{return resources_.material(candidate);}
} // namespace iiActorObject::object3d_detail
