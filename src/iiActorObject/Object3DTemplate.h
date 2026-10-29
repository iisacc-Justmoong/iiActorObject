#pragma once
#include <iiActorObject/Object3DEngine.h>
#include <iiActorObject/SurfaceBinding.h>
#include <iiActorObject/Object3DPoseRig.h>
#include <iiActorObject/SurfaceFittingRules.h>

namespace iiActorObject
{
struct TemplateValue final { std::string name;double value=0;bool operator==(const TemplateValue &) const=default; };
struct TemplateTarget final
{
    std::string name;
    std::size_t mesh=0;
    std::vector<TopologyPointDelta> deltas;
    double minimum=0,maximum=1;
};
struct TemplateAnchor final { std::size_t mesh=0;std::vector<std::uint32_t> points; };
struct TemplateJointBinding final
{
    BoneIndex bone=NoBone;
    TemplateAnchor head,tail;
    std::optional<std::array<TemplateAnchor,3>> rotationPlane;
};
struct TemplateJointFrame final
{
    BoneIndex bone=NoBone;
    Vector3 head,tail;
    std::optional<std::array<Vector3,3>> rotationPlane;
};
struct TemplateControl final
{
    std::string name;
    double minimum=-1,maximum=1;
    std::string label,group;
    bool operator==(const TemplateControl &) const=default;
};
struct TemplateControlKnot final
{
    double value=0,weight=0;
    bool operator==(const TemplateControlKnot &) const=default;
};
struct TemplateControlFactor final
{
    std::string control;
    std::vector<TemplateControlKnot> knots;
    bool operator==(const TemplateControlFactor &) const=default;
};
struct TemplateTargetDriver final
{
    std::size_t target=0;
    double scale=1;
    std::vector<TemplateControlFactor> factors;
    bool operator==(const TemplateTargetDriver &) const=default;
};
struct TemplateGrounding final
{
    std::size_t mesh=0;
    Vector3 normal{0,1,0};
    double offset=0;
    // Empty selects visible points; otherwise these source IDs define support.
    std::vector<std::uint32_t> supportPoints;
};
struct Object3DTemplateDefinition final
{
    Object3DHandle base;
    std::vector<TemplateTarget> targets;
    std::vector<TemplateJointBinding> joints;
    std::vector<SurfaceBinding> surfaces;
    std::vector<TemplateControl> controls;
    std::vector<TemplateTargetDriver> drivers;
    std::optional<TemplateGrounding> grounding;
    std::optional<Object3DPoseProgram> poseProgram;
    std::vector<SurfaceFittingRule> fittings;
    // Empty means object-space axes; otherwise one optional basis per surface binding.
    std::vector<std::optional<SurfaceBindingBasis>> surfaceOffsetBases;
};
// Ground visible or explicitly selected source points, translating the whole
// object and root joints once. Hidden helpers retain their original IDs.
[[nodiscard]] IIACTOROBJECT_EXPORT Vector3 groundObject3D(Object3DData &data,const TemplateGrounding &grounding);
// Updates local bind frames from source-point anchors. Strong exception guarantee.
IIACTOROBJECT_EXPORT void bindObject3DTemplateJoints(Object3DData &data,
    std::span<const TemplateJointBinding> joints);
// Object-space joint frames without requiring those positions to be mesh vertices.
IIACTOROBJECT_EXPORT void bindObject3DJointFrames(Object3DData &data,
    std::span<const TemplateJointFrame> joints);
// Core-owned immutable generation program. Shape catalog is distinct from runtime morphs.
class IIACTOROBJECT_EXPORT Object3DTemplate final
{
  public:
    Object3DTemplate(const Object3DTemplate &)=delete;
    Object3DTemplate &operator=(const Object3DTemplate &)=delete;
    [[nodiscard]] const Object3DTemplateDefinition &definition() const noexcept;
    // Raw target weights plus neutral-subtracted products of control curves.
    // Result order matches definition().targets; no asset is published.
    [[nodiscard]] std::vector<double> resolveValues(std::span<const TemplateValue> values) const;
    // Evaluates a detached value snapshot without registering a new engine asset.
    [[nodiscard]] Object3DData snapshot(std::span<const TemplateValue> values={}) const;
    // Always evaluates from the neutral source, with no accumulated slider drift.
    [[nodiscard]] Object3DHandle generate(std::span<const TemplateValue> values={},std::string name={}) const;
  private:
    friend class Object3DReference;
    friend class Object3DEditor;
    friend struct object3d_detail::ProjectAccess;
    friend struct object3d_detail::TemplateMorphBakeAccess;
    Object3DTemplate(Object3DReference engine,Object3DTemplateDefinition definition);
    [[nodiscard]] Object3DData evaluate(std::span<const TemplateValue> values) const;
    [[nodiscard]] Object3DData evaluateFinal(std::span<const TemplateValue> values) const;
    Object3DReference engine_;
    Object3DTemplateDefinition definition_;
};
} // namespace iiActorObject
