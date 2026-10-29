#pragma once
#include <iiActorObject/GlbDocument.h>
#include <iiActorObject/Animation.h>
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
struct GlbAnimationExportOptions final
{
    bool allowDiscardScalarCurves = false;
    std::size_t maximumKeys = 1000000;
};
struct GlbMorphCurveBinding final
{
    std::string_view curve;
    std::string_view morph;
    // Empty targets every matching mesh; otherwise select this exact mesh name.
    std::string_view mesh{};
};
struct GlbMorphAnimationOptions final
{
    // Views are borrowed for this call. A curve can drive multiple targets;
    // each morph on a particular mesh can be driven by only one curve.
    std::span<const GlbMorphCurveBinding> bindings;
    bool matchUnmappedCurvesByName = false;
    std::size_t maximumWeightValues = 16'000'000;
    std::size_t maximumCurveKeyVisits = 16'000'000;
    std::size_t maximumMappingEntries = 1'000'000;
    // Curve, mesh and logical morph indices; borrowed during export.
    std::span<const AnimationMorphBinding> indexedBindings;
};
IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportObject3DGlb(
    const Object3DAsset &asset, const AnimationClip &clip,
    const GlbAnimationExportOptions &options = {});
// Explicitly maps scalar curves to morph weights. Unmatched curves are rejected
// unless options.allowDiscardScalarCurves is true.
IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportObject3DGlb(
    const Object3DAsset &asset, const AnimationClip &clip,
    const GlbAnimationExportOptions &options, const GlbMorphAnimationOptions &morphOptions);
struct GlbImageRequest final
{
    // Views are borrowed for resolve() only. An empty URI denotes embedded data.
    std::string_view name, uri, mimeType;
    std::span<const std::uint8_t> embeddedBytes;
    std::size_t remainingBytes = 0;
};
struct GlbResolvedImage final
{
    // The core stores PNG; the host may resolve a URI or convert another format.
    std::vector<std::uint8_t> pngBytes;
    std::string notice;
};
class IIACTOROBJECT_EXPORT GlbImageResolver
{
  public:
    virtual ~GlbImageResolver() = default;
    [[nodiscard]] virtual std::optional<GlbResolvedImage> resolve(const GlbImageRequest &request) const = 0;
};
struct GlbImportOptions final
{
    GlbReadOptions document;
    std::size_t maximumNodes = 100000;
    std::size_t maximumMeshes = 4096;
    std::size_t maximumPrimitives = 16384;
    std::size_t maximumVertices = 2'000'000;
    std::size_t maximumTriangles = 2'000'000;
    std::size_t maximumBones = 4096;
    std::size_t maximumSkins = 4096;
    // Sum of skin.joints entries across the selected skins, including shared joints.
    std::size_t maximumSkinJointMappings = 1'000'000;
    std::size_t maximumMorphsPerMesh = 4096;
    std::size_t maximumMorphDeltas = 16'000'000;
    std::size_t maximumInfluencesPerVertex = 64;
    std::size_t maximumInfluenceSets = 256;
    std::size_t maximumUvSets = 32;
    std::size_t maximumImageBytes = 64 * 1024 * 1024;
    std::size_t maximumImageResolverRequests = 4096;
    double maximumJointMatrixError = 0.00001;
    // Loss must be explicitly requested; every discarded feature is reported.
    bool allowDiscardAnimations = false;
    bool allowDiscardUnsupportedAttributes = false;
    const GlbImageResolver *imageResolver = nullptr;
};
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
// Path fields are untrusted FBX data, not paths opened by iiActorObject.
// Views expire when resolve() returns. Embedded bytes are supplied for non-PNG
// content; an empty span denotes a reference to be resolved by the host.
// The resolver is borrowed for this import only.
struct FbxImageRequest final
{
    std::string_view textureName,filename,absoluteFilename,relativeFilename;
    std::size_t remainingBytes=0;
    std::span<const std::uint8_t> embeddedBytes;
};
struct FbxResolvedImage final
{
    std::vector<std::uint8_t> bytes;
    std::string mimeType="image/png";
    // Optional bounded diagnostic when the resolver changed the original resource.
    std::string notice;
};
class IIACTOROBJECT_EXPORT FbxImageResolver
{
  public:
    virtual ~FbxImageResolver()=default;
    [[nodiscard]] virtual std::optional<FbxResolvedImage> resolve(const FbxImageRequest &request) const=0;
};
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
