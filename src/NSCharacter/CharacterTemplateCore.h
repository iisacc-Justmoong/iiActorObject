#pragma once
#include <iiActorObject/Object3DTemplate.h>
#include <iiActorObject/Object3DTemplateRigTransfer.h>
#include <iiActorObject/Object3DTemplateAdapt.h>
#include <iiActorObject/Object3DTemplateMorphs.h>
#include <iiActorObject/Object3DPoseMorphs.h>
#include <iiActorObject/Object3DPoseCorrectives.h>
#include <iiActorObject/NSCharacter/CharacterAsset.h>
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#include <memory>
#include <span>
#include <string>

namespace iiActorObject::NSCharacter
{
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
} // namespace iiActorObject::NSCharacter
