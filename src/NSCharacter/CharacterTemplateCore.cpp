#include "CharacterTemplateCore.h"
#include <memory>
#include <stdexcept>
#include <utility>

namespace iiActorObject::NSCharacter
{
CharacterTemplate::CharacterTemplate(Object3DTemplateHandle program):program_(std::move(program))
{
    if(!program_.valid())throw std::invalid_argument("Character template requires a live core program");
}
bool CharacterTemplate::valid() const noexcept { return program_.valid(); }
CharacterEditor CharacterTemplate::edit(std::span<const TemplateValue> values,std::size_t historyLimit) const
{
    return CharacterEditor(program_.get().definition().base.engine().editTemplate(program_,values,historyLimit));
}
const Object3DTemplateHandle &CharacterTemplate::program() const noexcept { return program_; }
std::span<const TemplateControl> CharacterTemplate::controls() const { return program_.get().definition().controls; }
std::shared_ptr<const CharacterAsset> CharacterTemplate::generate(std::span<const TemplateValue> values,std::string name) const
{
    return std::make_shared<const CharacterAsset>(program_.get().generate(values,std::move(name)));
}
CharacterRigTransferResult CharacterTemplate::rigMesh(const Object3DHandle &target,const TemplateRigTransferOptions &options) const
{return rigMesh(target,options,SkinningProfileTransfer::SourceIfNonlinear);}
CharacterRigTransferResult CharacterTemplate::rigMesh(const Object3DHandle &target,const TemplateRigTransferOptions &options,SkinningProfileTransfer profile) const
{
    auto result=transferObject3DTemplateRig(program_,target,options,profile);
    const auto owner=result.object.engine();const auto id=result.object.id();
    try{return {std::make_shared<const CharacterAsset>(std::move(result.object)),std::move(result.report)};}
    catch(...){owner.eraseObject(id);throw;}
}
TemplateAdaptResult CharacterTemplate::adaptMesh(const Object3DHandle &target,const TemplateAdaptOptions &options) const
{
    return adaptObject3DTemplate(program_,target,options);
}
TemplateAdaptResult CharacterTemplate::adaptMesh(const Object3DHandle &target,const TemplateAdaptOptions &options,SkinningProfileTransfer profile) const
{return adaptObject3DTemplate(program_,target,options,profile);}
TemplateAdaptResult CharacterTemplate::adaptMesh(const Object3DHandle &target,const TemplateAdaptOptions &options,
    SkinningProfileTransfer rigProfile,SkinningProfileTransfer followerProfile) const
{return adaptObject3DTemplate(program_,target,options,rigProfile,followerProfile);}
TemplateAdaptResult CharacterTemplate::adaptMeshWithMatching(const Object3DHandle &target,const TemplateAdaptOptions &options,
    const TemplateAdaptMatchingOptions &matching) const
{return adaptObject3DTemplateWithMatching(program_,target,options,matching);}
TemplateAdaptResult CharacterTemplate::adaptMesh(const Object3DHandle &target,const TemplateAdaptOptions &options,
    SkinningProfileTransfer rigProfile,SkinningProfileTransfer followerProfile,const TemplateAdaptMatchingOptions &matching) const
{return adaptObject3DTemplate(program_,target,options,rigProfile,followerProfile,matching);}
TemplateAdaptResult CharacterTemplate::adaptMeshWithCorrespondence(const Object3DHandle &target,const TemplateAdaptOptions &options,
    const TemplateAdaptCorrespondenceOptions &correspondence) const
{return adaptObject3DTemplateWithCorrespondence(program_,target,options,correspondence);}
TemplateAdaptResult CharacterTemplate::adaptMeshWithCorrespondence(const Object3DHandle &target,const TemplateAdaptOptions &options,
    SkinningProfileTransfer rigProfile,SkinningProfileTransfer followerProfile,const TemplateAdaptCorrespondenceOptions &correspondence) const
{return adaptObject3DTemplateWithCorrespondence(program_,target,options,rigProfile,followerProfile,correspondence);}
TemplateMorphBakeResult CharacterTemplate::bakeMorphs(std::span<const TemplateValue> baseline,
    std::span<const TemplateMorphSample> samples,const TemplateMorphBakeOptions &options) const
{
    return bakeObject3DTemplateMorphs(program_,baseline,samples,options);
}
Object3DRigHandle CharacterTemplate::createFaceRig(const CharacterAsset &asset,std::span<const BoneIndex> mapping) const
{
    const auto &definition=program_.get().definition();if(!definition.poseProgram)throw std::logic_error("This template has no face pose program");
    if(!definition.base.engine().owns(asset.object()))throw std::invalid_argument("Face rig requires the template's owning engine");
    return createObject3DPoseRig(asset.object(),*definition.poseProgram,mapping);
}
Object3DRigHandle CharacterTemplate::createPoseCorrectiveRig(const CharacterAsset &asset,
    const Object3DPoseCorrectiveProgram &program,std::span<const BoneIndex> mapping) const
{
    if(!program_.get().definition().base.engine().owns(asset.object()))
        throw std::invalid_argument("Pose corrective rig requires the template's owning engine");
    return createObject3DPoseCorrectiveRig(asset.object(),program,mapping);
}
RiggedCharacter CharacterTemplate::generateRigged(std::span<const TemplateValue> values,std::string name) const
{
    if(!program_.get().definition().poseProgram)throw std::logic_error("This template has no face pose program");auto asset=generate(values,std::move(name));
    try{return {asset,createFaceRig(*asset)};}catch(...){program_.get().definition().base.engine().eraseObject(asset->object().id());throw;}
}
std::shared_ptr<const CharacterAsset> CharacterTemplate::bakeFaceMorphs(const CharacterAsset &asset,
    std::span<const BoneIndex> mapping,const PoseMorphBakeOptions &options)const
{
    const auto &definition=program_.get().definition();if(!definition.poseProgram)throw std::logic_error("This template has no face pose program");const auto owner=definition.base.engine();
    if(!owner.owns(asset.object()))throw std::invalid_argument("Face bake requires the template's owning engine");
    return std::make_shared<const CharacterAsset>(createObject3DPoseMorphs(asset.object(),*definition.poseProgram,mapping,options));
}
std::shared_ptr<const CharacterAsset> CharacterTemplate::bakeReferenceFaceMorphs(const CharacterAsset &asset,
    std::span<const BoneIndex> mapping,const PoseMorphBakeOptions &options,const Object3DPoseSnapshotOptions &snapshotOptions)const
{
    const auto &definition=program_.get().definition();if(!definition.poseProgram)throw std::logic_error("This template has no face pose program");
    if(!definition.base.engine().owns(asset.object()))throw std::invalid_argument("Face bake requires the template's owning engine");
    return std::make_shared<const CharacterAsset>(createReferenceObject3DPoseMorphs(asset.object(),*definition.poseProgram,mapping,options,snapshotOptions));
}
} // namespace iiActorObject::NSCharacter
