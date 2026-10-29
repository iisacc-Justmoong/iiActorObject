#pragma once
// Detached project graph; owns values and graph-local indices, not engine sessions.
#include "Object3DAdjustmentData.h"
#include <iiActorObject/Animation.h>
#include <iiActorObject/MeshPointMask.h>
#include <iiActorObject/Object3DEditLayerData.h>
#include <iiActorObject/Object3DProgressiveMorphData.h>
#include <iiActorObject/Object3DPoseProgramData.h>
#include <iiActorObject/Object3DTemplateMorphData.h>
#include <iiActorObject/Object3DTemplateTypes.h>
#include <iiActorObject/SurfaceBindingCore.h>
#include <iiActorObject/SurfaceFitSettings.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace iiActorObject::object3d_detail
{
// A persisted template has a graph-local base index rather than an engine handle.
struct ProjectTemplateDefinition final
{
    std::vector<TemplateTarget> targets;
    std::vector<TemplateJointBinding> joints;
    std::vector<SurfaceBinding> surfaces;
    std::vector<TemplateControl> controls;
    std::vector<TemplateTargetDriver> drivers;
    std::optional<TemplateGrounding> grounding;
    std::optional<Object3DPoseProgram> poseProgram;
    std::vector<SurfaceFittingRule> fittings;
    std::vector<std::optional<SurfaceBindingBasis>> surfaceOffsetBases;
};
struct ProjectTemplate final {std::size_t base=0;ProjectTemplateDefinition definition;};
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
} // namespace iiActorObject::object3d_detail
