#pragma once
// Private graph for the codec and the engine's transactional import boundary.
#include "Object3DProject.h"
#include "Object3DAdjustment.h"
#include "Object3DDna.h"
#include "Object3DSelectionState.h"
#include "Object3DTemplateMorphs.h"
#include "Object3DProgressiveMorph.h"

namespace iiActorObject::object3d_detail
{
struct ProjectTemplate final {std::size_t base=0;Object3DTemplateDefinition definition;};
struct ProjectLayer final {Object3DEditLayerInfo info;std::size_t adjustment=0;std::optional<std::size_t> masks={};};
struct ProjectState final {std::size_t program=0;std::vector<TemplateValue> values;std::vector<ProjectLayer> layers;std::optional<PoseReference> referencePose;};
struct ProjectEntry final {std::size_t asset=0;std::string label;std::optional<std::size_t> layers;std::optional<std::size_t> selections;std::vector<std::optional<std::size_t>> selectionRemap;};
struct ProjectSelectionRecord final {std::string name;std::optional<std::size_t> masks;};
struct ProjectSelectionState final {std::vector<ProjectSelectionRecord> entries;};
struct ProjectRig final {std::size_t asset=0,source=0;std::vector<BoneIndex> joints;std::uint32_t kind=0;std::optional<std::size_t> templateProgram;};
struct ProjectAnimation final
{
    std::string name;
    double duration=0;
    std::vector<BoneTrack> tracks;
    std::vector<AnimationCurve> curves;
    std::vector<Transform> requiredDeformationBind;
    std::vector<AnimationMorphBinding> morphBindings;
    std::optional<double> sourceStartSeconds;
};
struct ProjectArchive final
{
    std::vector<Object3DData> assets;
    std::vector<ProjectTemplate> programs;
    std::vector<Adjustment> adjustments;
    std::vector<ProjectState> states;
    std::vector<ProjectEntry> history;
    std::vector<std::vector<std::uint8_t>> dna;
    std::vector<ProjectRig> rigs;
    std::vector<std::vector<MeshPointMask>> maskSets;
    std::vector<std::vector<std::uint8_t>> posePrograms;
    std::vector<ProjectSelectionState> selections;
    std::vector<TemplateMorphRigProgram> morphPrograms;
    std::vector<std::vector<std::uint8_t>> correctivePrograms;
    std::vector<ProjectAnimation> animations;
    std::vector<ProgressiveMorphProgram> progressiveMorphPrograms;
    std::size_t cursor=0,limit=32;
    std::uint64_t revision=1;
    Object3DLayerId nextLayer=1;
    // Codec envelope version, used only when validating history transitions.
    std::uint32_t formatVersion=8;
};
struct ProjectAccess final
{
    static ProjectArchive capture(const Object3DEditorHandle &,std::span<const Object3DRigHandle>);
    static Object3DProject commit(const Object3DReference &,ProjectArchive &,const Object3DProjectOptions &);
    static Object3DProject instantiate(const Object3DReference &,ProjectArchive &,const Object3DProjectOptions &);
};
} // namespace iiActorObject::object3d_detail
