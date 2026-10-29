#include "CharacterInterchange.h"
#include <stdexcept>
namespace iiActorObject::NSCharacter
{
std::vector<std::uint8_t> exportProject(const CharacterEditor &editor,std::span<const Object3DRigHandle> rigs,Object3DProjectOptions options)
{return exportObject3DProject(editor.session(),rigs,options);}
std::vector<std::uint8_t> exportProject(const CharacterProject &project,Object3DProjectOptions options)
{return exportObject3DProjectWithAnimations(project.editor.session(),project.animations,project.rigs,options);}
std::vector<std::uint8_t> exportProjectWithAnimations(const CharacterEditor &editor,
    std::span<const Object3DProjectAnimation> animations,std::span<const Object3DRigHandle> rigs,Object3DProjectOptions options)
{return exportObject3DProjectWithAnimations(editor.session(),animations,rigs,options);}
CharacterProject importProject(const Object3DReference &engine,std::span<const std::uint8_t> bytes,Object3DProjectOptions options)
{
    auto restored=importObject3DProject(engine,bytes,options);
    return {CharacterEditor(std::move(restored.editor)),std::move(restored.rigs),std::move(restored.animations)};
}
std::vector<std::uint8_t> exportGlb(const CharacterAsset &asset)
{
    return exportObject3DGlb(asset.object().get());
}
std::vector<std::uint8_t> exportGlb(const CharacterAsset &asset,const GlbExportOptions &options)
{
    return exportObject3DGlb(asset.object().get(),options);
}
std::vector<std::uint8_t> exportGlb(const CharacterAsset &asset,const AnimationClip &clip,
                                    const GlbAnimationExportOptions &options)
{
    return exportObject3DGlb(asset.object().get(),clip,options);
}
std::vector<std::uint8_t> exportGlb(const CharacterAsset &asset,const AnimationClip &clip,
                                    const GlbAnimationExportOptions &options,const GlbMorphAnimationOptions &morphOptions)
{
    return exportObject3DGlb(asset.object().get(),clip,options,morphOptions);
}
GlbCharacterImport importGlb(const Object3DReference &engine,std::span<const std::uint8_t> bytes,
                             const GlbImportOptions &options)
{
    auto imported=importObject3DGlb(engine,bytes,options);const auto id=imported.object.id();
    try
    {
        auto asset=std::make_shared<CharacterAsset>(std::move(imported.object));
        return {std::move(asset),std::move(imported.warnings),std::move(imported.sourceJointToBone),
                imported.maximumJointMatrixError,std::move(imported.skinMappings),std::move(imported.externalIdentity)};
    }
    catch(...){engine.eraseObject(id);throw;}
}
GlbCharacterImport importGlb(Object3DEngine &engine,std::span<const std::uint8_t> bytes,
                             const GlbImportOptions &options)
{return importGlb(engine.reference(),bytes,options);}
GlbAnimatedCharacterImport importGlbWithAnimations(const Object3DReference &engine,std::span<const std::uint8_t> bytes,
                                                    const GlbAnimationImportOptions &options)
{
    auto imported=importObject3DGlbWithAnimations(engine,bytes,options);const auto id=imported.asset.object.id();
    try
    {
        auto asset=std::make_shared<CharacterAsset>(std::move(imported.asset.object));
        return {std::move(asset),std::move(imported.animations),std::move(imported.nodes),
                std::move(imported.asset.warnings),std::move(imported.asset.skinMappings),
                imported.asset.maximumJointMatrixError,std::move(imported.asset.externalIdentity)};
    }
    catch(...){engine.eraseObject(id);throw;}
}
GlbAnimatedCharacterImport importGlbWithAnimations(Object3DEngine &engine,std::span<const std::uint8_t> bytes,
                                                    const GlbAnimationImportOptions &options)
{return importGlbWithAnimations(engine.reference(),bytes,options);}
Object3DExternalIdentity makeGlbExternalIdentity(const CharacterAsset &source,const GlbCharacterImport &snapshot,
                                                 std::string_view expectedDocument)
{
    if(!snapshot.asset||!snapshot.externalIdentity)throw std::invalid_argument("GLB external identity is unavailable");
    return makeObject3DGlbExternalIdentity(source.object().get(),snapshot.asset->object().get(),
                                          *snapshot.externalIdentity,expectedDocument);
}
std::vector<std::uint8_t> exportFbx(const CharacterAsset &asset)
{
    return exportObject3DFbx(asset.object().get());
}
std::vector<std::uint8_t> exportFbx(const CharacterAsset &asset,const FbxExportOptions &options)
{
    return exportObject3DFbx(asset.object().get(),options);
}
std::vector<std::uint8_t> exportFbx(const CharacterAsset &asset,const AnimationClip &clip,
                                    const FbxAnimationExportOptions &options)
{
    return exportObject3DFbx(asset.object().get(),clip,options);
}
FbxProgressiveMorphExport exportFbxWithProgressiveMorphs(const CharacterAsset &asset,
    const Object3DRigHandle &rig,const FbxExportOptions &options)
{
    if(rig.get().object()!=asset.object())throw std::invalid_argument("Progressive rig belongs to another character asset");
    return exportObject3DFbxWithProgressiveMorphs(rig,options);
}
FbxProgressiveMorphExport exportFbxWithProgressiveMorphs(const CharacterAsset &asset,
    const Object3DRigHandle &rig,const AnimationClip &clip,const FbxAnimationExportOptions &options)
{
    if(rig.get().object()!=asset.object())throw std::invalid_argument("Progressive rig belongs to another character asset");
    return exportObject3DFbxWithProgressiveMorphs(rig,clip,options);
}
FbxProgressiveMorphExport exportFbxWithProgressiveMorphs(const CharacterAsset &asset,
    const Object3DRigHandle &rig,std::span<const FbxImportedAnimation> takes,const FbxProgressiveTakesExportOptions &options)
{
    if(rig.get().object()!=asset.object())throw std::invalid_argument("Progressive rig belongs to another character asset");
    return exportObject3DFbxWithProgressiveMorphs(rig,takes,options);
}
FbxProgressiveMorphExport exportFbxWithProgressiveMorphs(const CharacterAsset &asset,
    const Object3DRigHandle &rig,std::span<const Object3DProjectAnimation> takes,const FbxProgressiveTakesExportOptions &options)
{
    if(rig.get().object()!=asset.object())throw std::invalid_argument("Progressive rig belongs to another character asset");
    return exportObject3DFbxWithProgressiveMorphs(rig,takes,options);
}
CharacterImport importFbx(const Object3DReference &engine, std::span<const std::uint8_t> bytes,
                          const FbxImportOptions &options)
{
    auto imported = importObject3DFbx(engine, bytes, options);
    return {std::make_shared<CharacterAsset>(std::move(imported.object)), std::move(imported.warnings)};
}
CharacterImport importFbx(Object3DEngine &engine, std::span<const std::uint8_t> bytes,
                          const FbxImportOptions &options)
{
    return importFbx(engine.reference(), bytes, options);
}
ProgressiveCharacterImport importFbxWithProgressiveMorphs(const Object3DReference &engine,
    std::span<const std::uint8_t> bytes,const FbxImportOptions &options)
{
    auto imported=importObject3DFbxWithProgressiveMorphs(engine,bytes,options);
    const auto objectId=imported.asset.object.id();const auto rigId=imported.rig.id();
    try
    {
        auto asset=std::make_shared<CharacterAsset>(imported.asset.object);
        return {std::move(asset),std::move(imported.rig),std::move(imported.asset.warnings)};
    }
    catch(...)
    {
        if(rigId)engine.eraseRig(rigId);
        engine.eraseObject(objectId);
        throw;
    }
}
ProgressiveCharacterImport importFbxWithProgressiveMorphs(Object3DEngine &engine,
    std::span<const std::uint8_t> bytes,const FbxImportOptions &options)
{return importFbxWithProgressiveMorphs(engine.reference(),bytes,options);}
ProgressiveAnimatedCharacterImport importFbxWithProgressiveMorphsAndAnimations(const Object3DReference &engine,
    std::span<const std::uint8_t> bytes,const FbxAnimationImportOptions &options)
{
    auto imported=importObject3DFbxWithProgressiveMorphsAndAnimations(engine,bytes,options);
    const auto objectId=imported.asset.object.id(),rigId=imported.rig.id();
    try
    {
        auto asset=std::make_shared<CharacterAsset>(imported.asset.object);
        return {std::move(asset),std::move(imported.rig),std::move(imported.animations),
                std::move(imported.asset.warnings),imported.asset.maximumJointMatrixError,std::move(imported.asset.externalIdentity)};
    }
    catch(...)
    {
        if(rigId)engine.eraseRig(rigId);
        engine.eraseObject(objectId);
        throw;
    }
}
ProgressiveAnimatedCharacterImport importFbxWithProgressiveMorphsAndAnimations(Object3DEngine &engine,
    std::span<const std::uint8_t> bytes,const FbxAnimationImportOptions &options)
{return importFbxWithProgressiveMorphsAndAnimations(engine.reference(),bytes,options);}
AnimatedCharacterImport importFbxWithAnimations(const Object3DReference &engine,
    std::span<const std::uint8_t> bytes,const FbxAnimationImportOptions &options)
{
    auto imported=importObject3DFbxWithAnimations(engine,bytes,options);
    const auto id=imported.asset.object.id();
    try
    {
        auto asset=std::make_shared<CharacterAsset>(std::move(imported.asset.object));
        return {std::move(asset),std::move(imported.animations),std::move(imported.asset.warnings),
                imported.asset.maximumJointMatrixError,std::move(imported.asset.externalIdentity)};
    }
    catch(...)
    {
        engine.eraseObject(id);
        throw;
    }
}
AnimatedCharacterImport importFbxWithAnimations(Object3DEngine &engine,
    std::span<const std::uint8_t> bytes,const FbxAnimationImportOptions &options)
{
    return importFbxWithAnimations(engine.reference(),bytes,options);
}
} // namespace iiActorObject::NSCharacter
