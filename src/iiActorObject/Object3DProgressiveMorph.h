#pragma once
#include <iiActorObject/Object3DRig.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DProject.h>

namespace iiActorObject
{
// At fullWeight this existing linear morph is fully applied. Zero is the
// implicit neutral shape; full weights must be finite, nonzero and increasing.
struct ProgressiveMorphKey final
{
    std::string target;
    double fullWeight=1;
    std::string sourceName; // Optional original shape name, for interchange provenance.
    bool operator==(const ProgressiveMorphKey &) const=default;
};
struct ProgressiveMorphChannel final
{
    std::string name;
    double defaultValue=0;
    std::vector<ProgressiveMorphKey> keys;
    std::string sourceName; // Optional source channel name.
    std::vector<std::string> sourceMeshes; // Source instances sharing this channel.
    bool operator==(const ProgressiveMorphChannel &) const=default;
};
struct ProgressiveMorphProgram final
{
    std::vector<ProgressiveMorphChannel> channels;
    bool operator==(const ProgressiveMorphProgram &) const=default;
};
// The core rig translates named controls into weights for existing linear
// morphs on all meshes. Evaluations outside the key range extrapolate and may
// produce negative or greater-than-one output weights; values are not clamped.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DRigHandle createObject3DProgressiveMorphRig(
    const Object3DHandle &object,const ProgressiveMorphProgram &program);
[[nodiscard]] IIACTOROBJECT_EXPORT ProgressiveMorphProgram inspectObject3DProgressiveMorphRig(
    const Object3DRigHandle &rig);
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
// Scalar curves in this reserved namespace bind to same-named progressive
// controls. The mapping survives the existing project animation format.
inline constexpr std::string_view Object3DProgressiveCurvePrefix="iiActorObject/progressive/";
[[nodiscard]] IIACTOROBJECT_EXPORT std::string object3DProgressiveControlCurveName(std::string_view control);
// Samples a clip and its progressive rig together. All binding and component
// validation completes before the destination is changed.
IIACTOROBJECT_EXPORT void applyObject3DProgressiveAnimationClip(
    const Object3DRigHandle &rig,const AnimationClip &clip,std::span<const AnimationMorphBinding> morphBindings,
    double seconds,Pose &destination,std::span<SkeletalMeshComponent *const> meshes,
    AnimationWrap wrap=AnimationWrap::Clamp);
struct FbxProgressiveMorphExport final
{
    std::vector<std::uint8_t> bytes;
    std::vector<std::string> warnings;
};
// Exports static channels for one mesh or a validated group of identical mesh
// instances. Source record identities and rig animation require the SDK project.
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
