#pragma once
#include <iiActorObject/NSCharacter/CharacterAsset.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/GlbAnimationSource.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#include <iiActorObject/Object3DProgressiveMorph.h>
namespace iiActorObject::NSCharacter
{
using FbxImportOptions = iiActorObject::FbxImportOptions;
using FbxExportOptions = iiActorObject::FbxExportOptions;
using FbxAnimationImportOptions = iiActorObject::FbxAnimationImportOptions;
using FbxAnimationExportOptions = iiActorObject::FbxAnimationExportOptions;
struct CharacterImport final
{
    std::shared_ptr<const CharacterAsset> asset;
    std::vector<std::string> warnings;
};
struct ProgressiveCharacterImport final
{
    std::shared_ptr<const CharacterAsset> asset;
    Object3DRigHandle rig;
    std::vector<std::string> warnings;
};
struct ProgressiveAnimatedCharacterImport final
{
    std::shared_ptr<const CharacterAsset> asset;
    Object3DRigHandle rig;
    std::vector<FbxImportedAnimation> animations;
    std::vector<std::string> warnings;
    double maximumJointMatrixError=0;
    Object3DExternalIdentity externalIdentity;
};
struct GlbCharacterImport final
{
    std::shared_ptr<const CharacterAsset> asset;
    std::vector<std::string> warnings;
    std::vector<BoneIndex> sourceJointToBone;
    double maximumJointMatrixError = 0;
    std::vector<GlbSkinMapping> skinMappings;
    std::optional<GlbExternalIdentity> externalIdentity;
};
struct CharacterProject final
{
    CharacterEditor editor;
    std::vector<Object3DRigHandle> rigs;
    std::vector<Object3DProjectAnimation> animations;
};
struct AnimatedCharacterImport final
{
    std::shared_ptr<const CharacterAsset> asset;
    std::vector<FbxImportedAnimation> animations;
    std::vector<std::string> warnings;
    double maximumJointMatrixError = 0;
    Object3DExternalIdentity externalIdentity;
};
struct GlbAnimatedCharacterImport final
{
    std::shared_ptr<const CharacterAsset> asset;
    std::vector<GlbAnimationSource> animations;
    std::vector<GlbAnimationNodeBinding> nodes;
    std::vector<std::string> warnings;
    std::vector<GlbSkinMapping> skinMappings;
    double maximumJointMatrixError = 0;
    std::optional<GlbExternalIdentity> externalIdentity;
};
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportProject(
    const CharacterEditor &editor,std::span<const Object3DRigHandle> rigs={},Object3DProjectOptions options={});
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportProject(
    const CharacterProject &project,Object3DProjectOptions options={});
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportProjectWithAnimations(
    const CharacterEditor &editor,std::span<const Object3DProjectAnimation> animations,
    std::span<const Object3DRigHandle> rigs={},Object3DProjectOptions options={});
[[nodiscard]] IIACTOROBJECT_EXPORT CharacterProject importProject(
    const Object3DReference &engine,std::span<const std::uint8_t> bytes,Object3DProjectOptions options={});
IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportGlb(const CharacterAsset &asset);
IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportGlb(const CharacterAsset &asset,const GlbExportOptions &options);
IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportGlb(const CharacterAsset &asset,const AnimationClip &clip,
                                                         const GlbAnimationExportOptions &options = {});
IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportGlb(const CharacterAsset &asset,const AnimationClip &clip,
    const GlbAnimationExportOptions &options,const GlbMorphAnimationOptions &morphOptions);
[[nodiscard]] IIACTOROBJECT_EXPORT GlbCharacterImport importGlb(const Object3DReference &engine,
    std::span<const std::uint8_t> bytes,const GlbImportOptions &options = {});
[[nodiscard]] IIACTOROBJECT_EXPORT GlbCharacterImport importGlb(Object3DEngine &engine,
    std::span<const std::uint8_t> bytes,const GlbImportOptions &options = {});
[[nodiscard]] IIACTOROBJECT_EXPORT GlbAnimatedCharacterImport importGlbWithAnimations(
    const Object3DReference &engine,std::span<const std::uint8_t> bytes,const GlbAnimationImportOptions &options = {});
[[nodiscard]] IIACTOROBJECT_EXPORT GlbAnimatedCharacterImport importGlbWithAnimations(
    Object3DEngine &engine,std::span<const std::uint8_t> bytes,const GlbAnimationImportOptions &options = {});
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DExternalIdentity makeGlbExternalIdentity(
    const CharacterAsset &source,const GlbCharacterImport &snapshot,std::string_view expectedDocument);
IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportFbx(const CharacterAsset &asset);
IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportFbx(const CharacterAsset &asset,const FbxExportOptions &options);
IIACTOROBJECT_EXPORT std::vector<std::uint8_t> exportFbx(const CharacterAsset &asset,const AnimationClip &clip,
                                                         const FbxAnimationExportOptions &options = {});
[[nodiscard]] IIACTOROBJECT_EXPORT FbxProgressiveMorphExport exportFbxWithProgressiveMorphs(
    const CharacterAsset &asset,const Object3DRigHandle &rig,const FbxExportOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT FbxProgressiveMorphExport exportFbxWithProgressiveMorphs(
    const CharacterAsset &asset,const Object3DRigHandle &rig,const AnimationClip &clip,
    const FbxAnimationExportOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT FbxProgressiveMorphExport exportFbxWithProgressiveMorphs(
    const CharacterAsset &asset,const Object3DRigHandle &rig,std::span<const FbxImportedAnimation> takes,
    const FbxProgressiveTakesExportOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT FbxProgressiveMorphExport exportFbxWithProgressiveMorphs(
    const CharacterAsset &asset,const Object3DRigHandle &rig,std::span<const Object3DProjectAnimation> takes,
    const FbxProgressiveTakesExportOptions &options={});
IIACTOROBJECT_EXPORT CharacterImport importFbx(const Object3DReference &engine,
                                               std::span<const std::uint8_t> bytes,
                                               const FbxImportOptions &options = {});
IIACTOROBJECT_EXPORT CharacterImport importFbx(Object3DEngine &engine, std::span<const std::uint8_t> bytes,
                                               const FbxImportOptions &options = {});
[[nodiscard]] IIACTOROBJECT_EXPORT ProgressiveCharacterImport importFbxWithProgressiveMorphs(
    const Object3DReference &engine,std::span<const std::uint8_t> bytes,const FbxImportOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT ProgressiveCharacterImport importFbxWithProgressiveMorphs(
    Object3DEngine &engine,std::span<const std::uint8_t> bytes,const FbxImportOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT ProgressiveAnimatedCharacterImport importFbxWithProgressiveMorphsAndAnimations(
    const Object3DReference &engine,std::span<const std::uint8_t> bytes,const FbxAnimationImportOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT ProgressiveAnimatedCharacterImport importFbxWithProgressiveMorphsAndAnimations(
    Object3DEngine &engine,std::span<const std::uint8_t> bytes,const FbxAnimationImportOptions &options={});
IIACTOROBJECT_EXPORT AnimatedCharacterImport importFbxWithAnimations(const Object3DReference &engine,
    std::span<const std::uint8_t> bytes,const FbxAnimationImportOptions &options = {});
IIACTOROBJECT_EXPORT AnimatedCharacterImport importFbxWithAnimations(Object3DEngine &engine,
    std::span<const std::uint8_t> bytes,const FbxAnimationImportOptions &options = {});
} // namespace iiActorObject::NSCharacter
