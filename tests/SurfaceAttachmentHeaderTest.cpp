#include <iiActorObject/SurfaceAttachment.h>
#include <type_traits>

static_assert(std::is_default_constructible_v<iiActorObject::SurfaceAttachmentOptions>);
int surfaceAttachmentHeaderProbe()
{
    using Function = iiActorObject::SkinTransferResult (*)(iiActorObject::Object3DData &,
        const iiActorObject::SurfaceBinding &, std::size_t, iiActorObject::BoneIndex);
    return static_cast<Function>(&iiActorObject::transferMeshBindingWeights) ? 0 : 1;
}
