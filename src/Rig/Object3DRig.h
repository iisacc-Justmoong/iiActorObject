#pragma once
#include <iiActorObject/Object3DEngine.h>

namespace iiActorObject
{
namespace object3d_detail {struct TemplateMorphRigAccess;struct PoseCorrectiveRigAccess;struct ProgressiveMorphRigAccess;}
struct NamedRigControl final
{
    std::string_view name;
    double value = 0;
};
struct Object3DRigDefinition final
{
    std::string sourceFormat = "generic";
    std::vector<std::string> joints, rawControls, guiControls, blendShapes, animatedMaps;
    std::vector<double> rawControlDefaults, guiControlDefaults;
    std::size_t lodCount = 1;
    // nullopt replaces the full pose; present indices overlay only these driver joints.
    std::optional<std::vector<std::size_t>> drivenJoints;
};
struct Object3DJointDelta final
{
    Vector3 translation;
    Quaternion rotation;
    Vector3 scale; // Additive scale delta; zero means unchanged.
};
struct Object3DRigOutputs final
{
    std::vector<Object3DJointDelta> joints; // Definition joint order.
    std::vector<double> blendShapes, animatedMaps;
};
struct Object3DRigEvaluation final
{
    Object3DHandle object;
    std::size_t lod = 0;
    std::vector<Transform> joints; // Full core skeleton order, bind + evaluated delta.
    std::vector<double> blendShapes, animatedMaps;
};
struct RigSourceExportOptions final
{
    bool requireAllMeshes = false;
    std::size_t maximumBytes = 256 * 1024 * 1024;
    std::size_t maximumMemory = 1024ULL * 1024 * 1024;
    bool allowDiscardMorphNormals = false;
    bool allowDiscardAdditionalUvSets = false;
};
struct RigSourceExportResult final
{
    std::vector<std::byte> bytes;
    std::vector<std::string> warnings;
};
struct RigSourceSnapshot final
{
    const Object3DAsset *asset = nullptr;
    std::span<const BoneIndex> joints;
};

// Replaceable program backend. The core owns each driver and its instance buffers.
class IIACTOROBJECT_EXPORT Object3DRigDriver
{
  public:
    virtual ~Object3DRigDriver();
    [[nodiscard]] virtual const Object3DRigDefinition &definition() const noexcept = 0;
    [[nodiscard]] virtual std::unique_ptr<Object3DRigDriver> clone() const = 0;
    // Backends depending on bind frames can rebuild their target binding on clone.
    [[nodiscard]] virtual std::unique_ptr<Object3DRigDriver> cloneFor(const RigSourceSnapshot &source) const;
    [[nodiscard]] virtual Object3DRigOutputs evaluate(std::span<const double> controls,
                                                     RigControlSpace space, std::size_t lod) = 0;
    // Original source, when retained by a backend; not a serialization of later mesh edits.
    [[nodiscard]] virtual std::vector<std::byte> sourceBytes() const;
    [[nodiscard]] virtual RigSourceExportResult serializeSource(
        std::span<const RigSourceSnapshot> snapshots, RigSourceExportOptions options) const;
};

class IIACTOROBJECT_EXPORT Object3DRigSession final
{
  public:
    Object3DRigSession(const Object3DRigSession &) = delete;
    Object3DRigSession &operator=(const Object3DRigSession &) = delete;
    Object3DRigSession(Object3DRigSession &&) = delete;
    Object3DRigSession &operator=(Object3DRigSession &&) = delete;
    [[nodiscard]] const Object3DHandle &object() const noexcept;
    [[nodiscard]] const Object3DRigDefinition &definition() const noexcept;
    [[nodiscard]] std::span<const BoneIndex> jointMapping() const noexcept;
    // Each call supplies the whole chosen control buffer. Values are never silently clamped.
    [[nodiscard]] Object3DRigEvaluation evaluate(std::span<const double> controls,
        RigControlSpace space = RigControlSpace::Raw, std::size_t lod = 0);
    [[nodiscard]] Object3DRigEvaluation evaluateNamed(std::span<const NamedRigControl> controls,
        RigControlSpace space = RigControlSpace::Raw, std::size_t lod = 0);
    // Resolve omitted named controls to driver defaults without evaluating it.
    [[nodiscard]] std::vector<double> resolveNamed(std::span<const NamedRigControl> controls,
        RigControlSpace space = RigControlSpace::Raw) const;
    void applyTo(iiCharacter &actor, const Object3DRigEvaluation &evaluation) const;
    // Optional map is in driver joint order, targeting the new core bone indices.
    [[nodiscard]] Object3DRigHandle cloneFor(const Object3DHandle &object,
        std::span<const BoneIndex> jointMapping = {}) const;
    [[nodiscard]] std::vector<std::byte> sourceBytes() const;
    // Additional sessions contribute edited mesh LODs to the same source document.
    [[nodiscard]] RigSourceExportResult serializeSource(
        std::span<const Object3DRigHandle> additional = {}, RigSourceExportOptions options = {}) const;

  private:
    friend class Object3DReference;
    friend struct object3d_detail::ProjectAccess;
    friend struct object3d_detail::RigSamplingAccess;
    friend struct object3d_detail::TemplateMorphRigAccess;
    friend struct object3d_detail::PoseCorrectiveRigAccess;
    friend struct object3d_detail::ProgressiveMorphRigAccess;
    Object3DRigSession(Object3DHandle object, std::unique_ptr<Object3DRigDriver> driver,
        std::span<const BoneIndex> jointMapping);
    Object3DHandle object_;
    std::unique_ptr<Object3DRigDriver> driver_;
    Object3DRigDefinition definition_;
    std::vector<BoneIndex> joints_;
};
} // namespace iiActorObject
