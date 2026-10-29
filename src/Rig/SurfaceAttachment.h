#pragma once
#include <iiActorObject/SurfaceBindingCore.h>
#include <iiActorObject/SkinBinding.h>

namespace iiActorObject
{
struct SurfaceAttachmentOptions final
{
    SurfaceBindingOptions binding;
    SkinTransferOptions skin{.includeUnusedPoints=false};
    bool transferSkin=true;
};
struct SurfaceAttachmentResult final {std::size_t mesh=0;SkinTransferResult skin;};
// Nonnegative parts of geometric coefficients blend source skin rows. Zero cap
// preserves all influences; missing source skin needs an explicit fallback.
[[nodiscard]] IIACTOROBJECT_EXPORT SkinTransferResult transferMeshBindingWeights(Object3DData &data,
    const SurfaceBinding &binding,std::size_t maximumInfluences=0,BoneIndex fallback=NoBone);
// Optional profile transfer follows the same binding and contributing skin rows.
// Unbound points and fallback-only points retain their target profile value.
[[nodiscard]] IIACTOROBJECT_EXPORT SkinTransferResult transferMeshBindingWeights(Object3DData &data,
    const SurfaceBinding &binding,std::size_t maximumInfluences,BoneIndex fallback,SkinningProfileTransfer profile);
[[nodiscard]] IIACTOROBJECT_EXPORT SkinTransferResult transferMeshBindingWeightsWithBasis(Object3DData &data,
    const SurfaceBinding &binding,const SurfaceBindingBasis &basis,std::size_t maximumInfluences=0,
    BoneIndex fallback=NoBone,SkinningProfileTransfer profile=SkinningProfileTransfer::PreserveTarget);
} // namespace iiActorObject
