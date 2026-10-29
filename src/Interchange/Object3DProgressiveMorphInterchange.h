#pragma once
#include <iiActorObject/Object3DProgressiveMorphCore.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DProjectAnimationData.h>

namespace iiActorObject
{
struct FbxProgressiveMorphImport final
{
    Object3DImport asset;
    // Empty if the source has no progressive channels.
    Object3DRigHandle rig;
};
// Imports intermediate shapes as uniquely named core morphs and returns their
// nonlinear rig. This path does not import animation.
[[nodiscard]] IIACTOROBJECT_EXPORT FbxProgressiveMorphImport importObject3DFbxWithProgressiveMorphs(
    const Object3DReference &engine,std::span<const std::uint8_t> bytes,const FbxImportOptions &options={});
struct FbxProgressiveAnimatedImport final
{
    Object3DImport asset;
    Object3DRigHandle rig; // Empty if no progressive channels exist.
    std::vector<FbxImportedAnimation> animations;
};
// Imports skeleton, ordinary morphs, progressive controls and animation takes
// in one transaction; the returned rig and asset remain engine-owned.
[[nodiscard]] IIACTOROBJECT_EXPORT FbxProgressiveAnimatedImport importObject3DFbxWithProgressiveMorphsAndAnimations(
    const Object3DReference &engine,std::span<const std::uint8_t> bytes,const FbxAnimationImportOptions &options={});
struct FbxProgressiveMorphExport final
{
    std::vector<std::uint8_t> bytes;
    std::vector<std::string> warnings;
};
// Exports progressive channels on one or more meshes. Identical compatible
// instances can share FBX geometry; other meshes get separate FBX channels.
// Shared-control restoration requires the SDK metadata. Source record
// identities and editable rig state still require the SDK project.
[[nodiscard]] IIACTOROBJECT_EXPORT FbxProgressiveMorphExport exportObject3DFbxWithProgressiveMorphs(
    const Object3DRigHandle &rig,const FbxExportOptions &options={});
// Exports one native clip with explicitly bound ordinary morphs and reserved
// progressive control curves. FBX take time starts at zero.
[[nodiscard]] IIACTOROBJECT_EXPORT FbxProgressiveMorphExport exportObject3DFbxWithProgressiveMorphs(
    const Object3DRigHandle &rig,const AnimationClip &clip,const FbxAnimationExportOptions &options={});
struct FbxProgressiveTakesExportOptions final
{
    // indexedBindings must be empty; each take supplies its own morphBindings.
    // maximumSamples applies to the combined output of all takes.
    FbxAnimationExportOptions animation;
    std::size_t maximumTakes=64;
};
// Exports each named take with its source start time rounded to an FBX tick.
[[nodiscard]] IIACTOROBJECT_EXPORT FbxProgressiveMorphExport exportObject3DFbxWithProgressiveMorphs(
    const Object3DRigHandle &rig,std::span<const FbxImportedAnimation> takes,
    const FbxProgressiveTakesExportOptions &options={});
// Project clips must refer to the rig's object. Missing source times use zero
// and are reported in the export warnings.
[[nodiscard]] IIACTOROBJECT_EXPORT FbxProgressiveMorphExport exportObject3DFbxWithProgressiveMorphs(
    const Object3DRigHandle &rig,std::span<const Object3DProjectAnimation> takes,
    const FbxProgressiveTakesExportOptions &options={});
} // namespace iiActorObject
