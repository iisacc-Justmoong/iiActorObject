#pragma once
#include <iiActorObject/GlbDocument.h>
#include <iiActorObject/GlbImageResolver.h>
#include <iiActorObject/Object3DGlbImportOptions.h>
#include <iiActorObject/FbxImageResolver.h>
#include <iiActorObject/Animation.h>
#include <iiActorObject/Object3DGlbAnimationOptions.h>
#include <iiActorObject/Object3DEngine.h>
#include <iiActorObject/Object3DExternalEdits.h>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace iiActorObject
{
class AnimationClip;
IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportObject3DGlb(const Object3DAsset &asset);
struct GlbExportOptions final
{
    // Nonempty opts into external-edit provenance. The token is not authentication.
    std::string externalDocument;
};
IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportObject3DGlb(
    const Object3DAsset &asset,const GlbExportOptions &options);
IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportObject3DGlb(
    const Object3DAsset &asset, const AnimationClip &clip,
    const GlbAnimationExportOptions &options = {});
// Explicitly maps scalar curves to morph weights. Unmatched curves are rejected
// unless options.allowDiscardScalarCurves is true.
IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportObject3DGlb(
    const Object3DAsset &asset, const AnimationClip &clip,
    const GlbAnimationExportOptions &options, const GlbMorphAnimationOptions &morphOptions);
struct GlbSkinMapping final
{
    std::size_t sourceSkinIndex = 0;
    // Source skin.joints index -> stable bone ID in the merged core asset.
    std::vector<BoneIndex> sourceJointToBone;
};
struct GlbExternalMeshIdentity final
{
    std::size_t mesh=0,source=0;
    bool pointsRestored=false;
};
struct GlbExternalIdentity final
{
    std::string document;
    std::vector<ExternalBoneIdentity> bones;
    std::vector<ExternalMaterialIdentity> materials;
    std::vector<GlbExternalMeshIdentity> meshes;
    // Root introduced by the GLB importer; this is not an authored source bone.
    std::optional<BoneIndex> syntheticRoot;
};
struct GlbImportResult final
{
    Object3DHandle object;
    std::vector<std::string> warnings;
    // First selected skin.joints index -> stable core bone index.
    std::vector<BoneIndex> sourceJointToBone;
    double maximumJointMatrixError = 0;
    // Selected skins in first mesh-use order; empty for an entirely static scene.
    std::vector<GlbSkinMapping> skinMappings;
    // Optional file provenance, not proof that an external tool retained all edits.
    std::optional<GlbExternalIdentity> externalIdentity;
};
[[nodiscard]] IIACTOROBJECT_EXPORT GlbImportResult importObject3DGlb(
    const Object3DReference &engine, std::span<const std::uint8_t> bytes,
    const GlbImportOptions &options = {});
// Derive complete triangle and corner identities only when source point IDs and
// provenance are available and every source triangle is unambiguous.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DExternalIdentity makeObject3DGlbExternalIdentity(
    const Object3DAsset &source,const Object3DAsset &snapshot,
    const GlbExternalIdentity &identity,std::string_view expectedDocument);
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DExternalIdentity makeObject3DGlbExternalIdentity(
    const Object3DAsset &source,const GlbImportResult &snapshot,std::string_view expectedDocument);
IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportObject3DFbx(const Object3DAsset &asset);
struct FbxExportOptions final
{
    // Permit a control-point projection in standard FBX while preserving exact per-layout normals in SDK metadata.
    bool allowMorphNormalMetadata = false;
    std::string externalDocument;
};
IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportObject3DFbx(const Object3DAsset &asset,const FbxExportOptions &options);
struct FbxAnimationExportOptions final
{
    FbxExportOptions geometry;
    // Scalar names become morph channels only when explicitly enabled.
    bool exportMatchingMorphCurves = false;
    bool allowDiscardScalarCurves = false;
    double maximumSampleInterval = 1.0 / 30.0;
    // Radians, evaluated at each key and at quarter points of every interval.
    double maximumRotationError = 0.001;
    // Total sample timestamps, including each repeated morph channel.
    std::size_t maximumSamples = 250000;
    // Explicit per-mesh logical morph targets, borrowed during export.
    std::span<const AnimationMorphBinding> indexedBindings;
};
IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportObject3DFbx(
    const Object3DAsset &asset, const AnimationClip &clip,
    const FbxAnimationExportOptions &options = {});
struct FbxImportOptions final
{
    std::size_t maxFileBytes = 256 * 1024 * 1024;
    std::size_t maxVertices = 2'000'000;
    std::size_t maxBones = 4096;
    std::size_t maxMorphsPerMesh = 4096;
    std::size_t maxPolygons = 2'000'000;
    std::size_t maxCorners = 8'000'000;
    bool restoreAuthoringMetadata = true;
    double maximumJointMatrixError = 1e-7;
    std::size_t maxImageBytes = 64*1024*1024;
    std::size_t maxUvSets = 32;
    // Total direct coordinates and per-corner UV indices across imported meshes.
    // Counts direct UV values, visible-corner indices and restored extra-set layout indices.
    std::size_t maxUvEntries = 16'000'000;
    // Resolves external references or converts unsupported embedded images to PNG.
    // The parser itself never opens external paths.
    const FbxImageResolver *imageResolver = nullptr;
    std::size_t maxImageResolverRequests = 4096;
};
struct Object3DImport final
{
    Object3DHandle object;
    std::vector<std::string> warnings;
    double maximumJointMatrixError = 0;
    Object3DExternalIdentity externalIdentity;
};
IIACTOROBJECT_EXPORT Object3DImport importObject3DFbx(const Object3DReference &engine,
                                                      std::span<const std::uint8_t> bytes,
                                                      const FbxImportOptions &options = {});
enum class FbxMorphAnimationGrouping { MergeByName, PerMesh };
struct FbxAnimationImportOptions final
{
    FbxImportOptions geometry;
    double maximumSampleInterval = 1.0 / 30.0;
    double maximumPositionError = 0.0001;
    double maximumRotationError = 0.001;
    double maximumScaleError = 0.0001;
    double maximumMorphError = 0.0001;
    std::size_t maximumKeys = 1000000;
    std::size_t maximumEvaluations = 10000;
    std::size_t maximumEvaluationValues = 4000000;
    std::size_t maximumClips = 64;
    bool allowDiscardUnsupportedAnimation = false;
    FbxMorphAnimationGrouping morphGrouping = FbxMorphAnimationGrouping::MergeByName;
};
struct FbxImportedAnimation final
{
    std::string name;
    double sourceStartSeconds = 0;
    std::shared_ptr<const AnimationClip> clip;
    std::vector<AnimationMorphBinding> morphBindings;
};
struct FbxAnimatedImport final
{
    Object3DImport asset;
    std::vector<FbxImportedAnimation> animations;
};
IIACTOROBJECT_EXPORT FbxAnimatedImport importObject3DFbxWithAnimations(
    const Object3DReference &engine, std::span<const std::uint8_t> bytes,
    const FbxAnimationImportOptions &options = {});
} // namespace iiActorObject
