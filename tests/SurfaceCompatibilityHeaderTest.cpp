#include <iiActorObject/SurfaceBinding.h>
#include <type_traits>

// The original entry point continues to supply both geometry and skin types.
static_assert(std::is_default_constructible_v<iiActorObject::Object3DData>);
static_assert(std::is_default_constructible_v<iiActorObject::SurfaceAttachmentOptions>);
static_assert(std::is_default_constructible_v<iiActorObject::SkinTransferOptions>);
int surfaceCompatibilityHeaderProbe()
{
    return iiActorObject::SurfaceBinding{}.points.empty() ? 0 : 1;
}
