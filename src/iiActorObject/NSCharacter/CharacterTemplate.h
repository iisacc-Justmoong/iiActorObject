#pragma once
#include <iiActorObject/Object3DTemplate.h>
#include <iiActorObject/Object3DTemplateRigTransfer.h>
#include <iiActorObject/Object3DTemplateAdapt.h>
#include <iiActorObject/Object3DTemplateMorphs.h>
#include <iiActorObject/Object3DPoseMorphs.h>
#include <iiActorObject/Object3DPoseCorrectives.h>
#include <iiActorObject/NSCharacter/CharacterAsset.h>
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace iiActorObject::NSCharacter
{
struct HumanSkinSelection;
struct HumanAccessoryInfo final
{
    std::string id,name,category;
};
struct HumanAccessorySelection final
{
    std::filesystem::path directory;
    std::vector<std::string> componentIds;
    bool hideCoveredBody=true;
};
// Reads the catalogue only; meshes, images and engine objects are not retained.
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<HumanAccessoryInfo> listMakeHumanAccessories(
    const std::filesystem::path &directory,std::size_t maximumBytes=1024*1024);
struct RiggedCharacter final
{
    std::shared_ptr<const CharacterAsset> asset;
    Object3DRigHandle rig;
};
struct CharacterRigTransferResult final
{
    std::shared_ptr<const CharacterAsset> asset;
    TemplateRigTransferReport report;
};
// A human view of the engine-owned template; no model, target, rig or engine storage.
class IIACTOROBJECT_EXPORT CharacterTemplate final
{
  public:
    explicit CharacterTemplate(Object3DTemplateHandle program);
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] const Object3DTemplateHandle &program() const noexcept;
    [[nodiscard]] std::span<const TemplateControl> controls() const;
    [[nodiscard]] std::shared_ptr<const CharacterAsset> generate(
        std::span<const TemplateValue> values={},std::string name={}) const;
    [[nodiscard]] CharacterEditor edit(std::span<const TemplateValue> values={},std::size_t historyLimit=32) const;
    [[nodiscard]] RiggedCharacter generateRigged(std::span<const TemplateValue> values={},std::string name={}) const;
    [[nodiscard]] CharacterRigTransferResult rigMesh(const Object3DHandle &target,
        const TemplateRigTransferOptions &options={}) const;
    [[nodiscard]] CharacterRigTransferResult rigMesh(const Object3DHandle &target,
        const TemplateRigTransferOptions &options,SkinningProfileTransfer profile) const;
    // The returned core-owned program can be wrapped by CharacterTemplate.
    [[nodiscard]] TemplateAdaptResult adaptMesh(const Object3DHandle &target,
        const TemplateAdaptOptions &options={}) const;
    [[nodiscard]] TemplateAdaptResult adaptMesh(const Object3DHandle &target,
        const TemplateAdaptOptions &options,SkinningProfileTransfer profile) const;
    [[nodiscard]] TemplateAdaptResult adaptMesh(const Object3DHandle &target,const TemplateAdaptOptions &options,
        SkinningProfileTransfer rigProfile,SkinningProfileTransfer followerProfile) const;
    [[nodiscard]] TemplateAdaptResult adaptMeshWithMatching(const Object3DHandle &target,const TemplateAdaptOptions &options,
        const TemplateAdaptMatchingOptions &matching) const;
    [[nodiscard]] TemplateAdaptResult adaptMesh(const Object3DHandle &target,const TemplateAdaptOptions &options,
        SkinningProfileTransfer rigProfile,SkinningProfileTransfer followerProfile,
        const TemplateAdaptMatchingOptions &matching) const;
    [[nodiscard]] TemplateAdaptResult adaptMeshWithCorrespondence(const Object3DHandle &target,
        const TemplateAdaptOptions &options,const TemplateAdaptCorrespondenceOptions &correspondence) const;
    [[nodiscard]] TemplateAdaptResult adaptMeshWithCorrespondence(const Object3DHandle &target,
        const TemplateAdaptOptions &options,SkinningProfileTransfer rigProfile,SkinningProfileTransfer followerProfile,
        const TemplateAdaptCorrespondenceOptions &correspondence) const;
    [[nodiscard]] TemplateMorphBakeResult bakeMorphs(std::span<const TemplateValue> baseline,
        std::span<const TemplateMorphSample> samples,const TemplateMorphBakeOptions &options={}) const;
    [[nodiscard]] Object3DRigHandle createFaceRig(const CharacterAsset &asset,std::span<const BoneIndex> mapping={}) const;
    [[nodiscard]] Object3DRigHandle createPoseCorrectiveRig(const CharacterAsset &asset,
        const Object3DPoseCorrectiveProgram &program,std::span<const BoneIndex> mapping={}) const;
    [[nodiscard]] std::shared_ptr<const CharacterAsset> bakeFaceMorphs(const CharacterAsset &asset,
        std::span<const BoneIndex> mapping={},const PoseMorphBakeOptions &options={}) const;
    [[nodiscard]] std::shared_ptr<const CharacterAsset> bakeReferenceFaceMorphs(const CharacterAsset &asset,
        std::span<const BoneIndex> mapping={},const PoseMorphBakeOptions &options={},
        const Object3DPoseSnapshotOptions &snapshotOptions={}) const;
  private:
    Object3DTemplateHandle program_;
};
// Adds and removes accessories, including mixed changes, in a same-engine
// template. Retained slots keep their relative order.
// The core engine retains ownership; incompatible source layouts are rejected.
[[nodiscard]] IIACTOROBJECT_EXPORT bool setHumanAccessories(CharacterEditor &editor,
    const CharacterTemplate &alternative,const std::filesystem::path &directory,
    std::size_t maximumCatalogueBytes=1024*1024);
// Explicitly permit dropping edits on removed accessories in the new state.
// Undo retains those edits in the preceding core-owned history snapshot.
[[nodiscard]] IIACTOROBJECT_EXPORT bool setHumanAccessories(CharacterEditor &editor,
    const CharacterTemplate &alternative,const std::filesystem::path &directory,
    const TemplateSurfaceRemovalOptions &removal,std::size_t maximumCatalogueBytes=1024*1024);
struct MakeHumanTemplateOptions final
{
    double scale=.1; // Original CC0 hm08 asset units -> metres.
    bool groundAtFeet=true;
    std::vector<std::string> visibleGroups{"body"};
    std::size_t maximumBytes=256*1024*1024;
    bool semanticControls=true;
    std::filesystem::path faceAssets; // Optional CC0 descriptor/image bundle.
    std::filesystem::path macroAssets; // Optional CC0 full morphology tensor bundle.
    std::filesystem::path expressionAssets; // Optional CC0 59-unit face pose program.
    std::optional<HumanAccessorySelection> accessories; // Optional authored hm08 accessories.
};
// Reads CC0 data only; decoding is transient, model/program ownership goes to the core.
[[nodiscard]] IIACTOROBJECT_EXPORT CharacterTemplate loadMakeHumanTemplate(
    const Object3DReference &engine,const std::filesystem::path &directory,
    const MakeHumanTemplateOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT CharacterTemplate loadMakeHumanTemplate(
    const Object3DReference &engine,const std::filesystem::path &directory,
    const MakeHumanTemplateOptions &options,const HumanSkinSelection &skin);
} // namespace iiActorObject::NSCharacter
