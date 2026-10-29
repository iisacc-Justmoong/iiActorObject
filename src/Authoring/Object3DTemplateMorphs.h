#pragma once
#include <iiActorObject/Object3DTemplateMorphData.h>
#include <iiActorObject/Object3DTemplate.h>

namespace iiActorObject
{
// At weight zero use baseline; at weight one use endpoint for this input only.
struct TemplateMorphSample final
{
    std::string input;
    double endpoint=0;
    std::string name;
};
struct TemplateMorphChannel final
{
    std::string input,name;
    double baseline=0,endpoint=0;
    std::vector<std::size_t> meshes;
    // Local bind transforms that differ at the endpoint; geometry morphs cannot move them.
    std::vector<BoneIndex> changedJoints;
    std::size_t deltas=0;
};
struct TemplateMorphBakeResult final
{
    Object3DHandle object;
    std::vector<TemplateMorphChannel> channels;
    std::size_t pointVisits=0,deltas=0;
    // Provenance is borrowed; an explicitly erased template may expire.
    Object3DTemplateHandle templateProgram;
    std::optional<std::vector<TemplateValue>> baselineInputs;
    std::optional<TemplateMorphBakeOptions> bakeOptions;
};
struct TemplateMorphRigInfo final
{
    Object3DTemplateHandle templateProgram;
    TemplateMorphRigProgram program;
};
// The core evaluates the full generation graph at both endpoints and publishes
// a separate engine-owned asset only after all samples succeed. Each channel
// uses a normalized [0,1] weight and may appear on several independent meshes.
[[nodiscard]] IIACTOROBJECT_EXPORT TemplateMorphBakeResult bakeObject3DTemplateMorphs(
    const Object3DTemplateHandle &program,std::span<const TemplateValue> baseline,
    std::span<const TemplateMorphSample> samples,const TemplateMorphBakeOptions &options={});
// One normalized control per baked channel, applied to all matching meshes via
// the core rig session. Existing pose joints are not changed.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DRigHandle createObject3DTemplateMorphRig(
    const TemplateMorphBakeResult &baked);
// Expose authored morph names as shared normalized [0,1] controls. A name on
// multiple meshes drives each matching channel without changing actor joints.
// Source template and bake provenance remain explicitly unknown.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DRigHandle createObject3DMorphChannelRig(const Object3DHandle &object);
// Returns the core-owned driver's detached metadata and borrowed source handle.
// Rejects other rig backends; an explicitly erased source can be invalid.
[[nodiscard]] IIACTOROBJECT_EXPORT TemplateMorphRigInfo inspectObject3DTemplateMorphRig(
    const Object3DRigHandle &rig);
// Evaluates the live source template at the combined named channel weights,
// updates the actor's instance bind and synchronizes its morphs. The actor's
// animation locals remain in the original asset skeleton's coordinate system.
IIACTOROBJECT_EXPORT void applyObject3DTemplateMorphRigWithBind(
    const Object3DRigHandle &rig,iiCharacter &actor,std::span<const NamedRigControl> controls);
} // namespace iiActorObject
