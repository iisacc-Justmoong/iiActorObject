#include "CharacterEditor.h"
#include <stdexcept>
namespace iiActorObject::NSCharacter
{
const Object3DEditorHandle &CharacterEditor::session() const noexcept {return editor_;}
CharacterEditor::CharacterEditor(std::shared_ptr<const CharacterAsset> asset, std::size_t historyLimit)
    : asset_(std::move(asset))
{
    if (!asset_ || !asset_->valid())
        throw std::invalid_argument("Character editor needs a live core object reference");
    editor_ = asset_->object().engine().editObject(asset_->object(), historyLimit);
    views_[asset_->object().id()] = asset_;
}
const std::shared_ptr<const CharacterAsset> &CharacterEditor::asset() const
{
    const auto &object = editor_.get().object();
    (void)object.get();
    if (!asset_ || asset_->object() != object)
    {
        auto &view = views_[object.id()];
        asset_ = view.lock();
        if (!asset_)
        {
            asset_ = std::make_shared<CharacterAsset>(object);
            view = asset_;
        }
        std::erase_if(views_, [](const auto &v) { return v.second.expired(); });
    }
    return asset_;
}
CharacterEditor::CharacterEditor(Object3DEditorHandle editor):editor_(std::move(editor))
{
    asset_=std::make_shared<CharacterAsset>(editor_.get().object());views_[asset_->object().id()]=asset_;
}
bool CharacterEditor::layered() const { return editor_.get().layered(); }
std::span<const TemplateValue> CharacterEditor::templateValues() const { return editor_.get().templateValues(); }
std::vector<Object3DEditLayerInfo> CharacterEditor::layers() const { return editor_.get().layers(); }
void CharacterEditor::setTemplateValues(std::span<const TemplateValue> values) { editor_.get().setTemplateValues(values); }
void CharacterEditor::setSkinningProfile(std::size_t mesh,SkinningProfile profile)
{ editor_.get().setSkinningProfile(mesh,std::move(profile)); }
void CharacterEditor::setLayerEnabled(Object3DLayerId id,bool enabled) { editor_.get().setLayerEnabled(id,enabled); }
void CharacterEditor::setLayerWeight(Object3DLayerId id,double weight) { editor_.get().setLayerWeight(id,weight); }
std::span<const MeshPointMask> CharacterEditor::layerPointMasks(Object3DLayerId id) const{return editor_.get().layerPointMasks(id);}
void CharacterEditor::setLayerPointMasks(Object3DLayerId id,std::span<const MeshPointMask> masks){editor_.get().setLayerPointMasks(id,masks);}
std::vector<std::string> CharacterEditor::pointSelectionNames() const{return editor_.get().pointSelectionNames();}
std::span<const MeshPointMask> CharacterEditor::pointSelection(std::string_view name) const{return editor_.get().pointSelection(name);}
void CharacterEditor::setPointSelection(std::string name,std::span<const MeshPointMask> masks){editor_.get().setPointSelection(std::move(name),masks);}
void CharacterEditor::renamePointSelection(std::string_view oldName,std::string newName){editor_.get().renamePointSelection(oldName,std::move(newName));}
void CharacterEditor::removePointSelection(std::string_view name){editor_.get().removePointSelection(name);}
void CharacterEditor::setLayerPointSelection(Object3DLayerId id,std::string_view name){editor_.get().setLayerPointSelection(id,name);}
void CharacterEditor::renameLayer(Object3DLayerId id,std::string label) { editor_.get().renameLayer(id,std::move(label)); }
void CharacterEditor::moveLayer(Object3DLayerId id,std::size_t index) { editor_.get().moveLayer(id,index); }
void CharacterEditor::removeLayer(Object3DLayerId id) { editor_.get().removeLayer(id); }
void CharacterEditor::detachTemplate() { editor_.get().detachTemplate(); }
SurfaceAttachmentResult CharacterEditor::attachSurface(CharacterMesh surface,std::size_t sourceMesh,const SurfaceAttachmentOptions &options)
{return editor_.get().attachSurface(std::move(surface),sourceMesh,options);}
std::span<const SurfaceFittingRule> CharacterEditor::surfaceFittings() const{return editor_.get().surfaceFittings();}
void CharacterEditor::setSurfaceFittings(std::span<const SurfaceFittingRule> rules){editor_.get().setSurfaceFittings(rules);}
std::uint64_t CharacterEditor::revision() const
{
    return editor_.get().revision();
}
std::string_view CharacterEditor::editLabel() const
{
    return editor_.get().editLabel();
}
void CharacterEditor::replace(CharacterData data, std::string label)
{
    editor_.get().replace(std::move(data), std::move(label));
}
Object3DSelectionTransferReport CharacterEditor::replaceWithProjectedSelections(const Object3DHandle &target,
    std::span<const MeshMaskProjectionPair> pairs,const Object3DSelectionTransferOptions &options,std::string label)
{return editor_.get().replaceWithProjectedSelections(target,pairs,options,std::move(label));}
void CharacterEditor::setReferencePose(std::optional<PoseReference> reference)
{
    editor_.get().setReferencePose(std::move(reference));
}
bool CharacterEditor::setMaterialSurface(std::size_t material,Object3DMaterialSurface surface)
{return editor_.get().setMaterialSurface(material,surface);}
Object3DExternalReport CharacterEditor::applyExternalEdits(const CharacterAsset &source,Object3DExternalSnapshot baseline,Object3DExternalSnapshot edited,const Object3DExternalOptions &options)
{return editor_.get().applyExternalEdits(source.object().get(),baseline,edited,options);}
void CharacterEditor::moveVertices(std::size_t mesh, std::span<const VertexMove> moves)
{
    editor_.get().moveVertices(mesh, moves);
}
void CharacterEditor::moveControlPoints(std::size_t mesh,std::span<const TopologyPointMove> moves)
{
    editor_.get().moveControlPoints(mesh,moves);
}
void CharacterEditor::setControlWeights(std::size_t mesh,std::span<const TopologyPointWeights> edits)
{
    editor_.get().setControlWeights(mesh,edits);
}
SurfaceBindResult CharacterEditor::bindSurfaceWeights(std::size_t mesh,std::span<const BoneEnvelope> envelopes,const SurfaceBindOptions &options)
{return editor_.get().bindSurfaceWeights(mesh,envelopes,options);}
SkeletonSurfaceBindResult CharacterEditor::bindSurfaceWeightsFromSkeleton(std::size_t mesh,
    const SkeletonEnvelopeOptions &inference,const SurfaceBindOptions &binding)
{return editor_.get().bindSurfaceWeightsFromSkeleton(mesh,inference,binding);}
SkinTransferResult CharacterEditor::transferWeights(std::size_t mesh,const CharacterAsset &source,std::size_t sourceMesh,const SkinTransferOptions &options)
{return editor_.get().transferWeights(mesh,source.object().get(),sourceMesh,options);}
SkinTransferResult CharacterEditor::transferWeights(std::size_t mesh,const CharacterAsset &source,std::size_t sourceMesh,const SkinTransferOptions &options,SkinningProfileTransfer profile)
{return editor_.get().transferWeights(mesh,source.object().get(),sourceMesh,options,profile);}
SkinNormalTransferResult CharacterEditor::transferWeightsWithNormalTies(std::size_t mesh,const CharacterAsset &source,
    std::size_t sourceMesh,const SkinTransferOptions &options,SkinningProfileTransfer profile,
    const SkinNormalTransferOptions &normals,SurfaceQueryBudget &budget)
{return editor_.get().transferWeightsWithNormalTies(mesh,source.object().get(),sourceMesh,options,profile,normals,budget);}
SkinTransferResult CharacterEditor::mirrorWeights(std::size_t mesh,std::span<const BoneIndex> boneMirrors,const SkinMirrorOptions &options)
{return editor_.get().mirrorWeights(mesh,boneMirrors,options);}
SkinTransferResult CharacterEditor::mirrorWeights(std::size_t mesh,std::span<const BoneIndex> boneMirrors,const SkinMirrorOptions &options,SkinningProfileTransfer profile)
{return editor_.get().mirrorWeights(mesh,boneMirrors,options,profile);}
void CharacterEditor::setControlMorph(std::size_t mesh,TopologyMorph morph)
{
    editor_.get().setControlMorph(mesh,std::move(morph));
}
void CharacterEditor::setDefaultMorphWeight(std::size_t mesh,std::string_view name,double value)
{
    editor_.get().setDefaultMorphWeight(mesh,name,value);
}
MeshSymmetryResult CharacterEditor::symmetrizeMesh(std::size_t mesh,const MeshSymmetryMap &mapping,const MeshSymmetryEditOptions &options)
{return editor_.get().symmetrizeMesh(mesh,mapping,options);}
MorphMirrorResult CharacterEditor::mirrorMorphs(std::size_t mesh,const MeshSymmetryMap &mapping,std::span<const MorphMirrorRequest> requests,const MorphMirrorOptions &options)
{return editor_.get().mirrorMorphs(mesh,mapping,requests,options);}
SurfaceFitResult CharacterEditor::fitSurface(std::size_t mesh,std::span<const SurfaceCollider *const> obstacles,const SurfaceFitOptions &options)
{return editor_.get().fitSurface(mesh,obstacles,options);}
SurfaceFitResult CharacterEditor::fitSurface(std::size_t mesh,const SurfaceCollider &obstacle,const SurfaceFitOptions &options)
{return editor_.get().fitSurface(mesh,obstacle,options);}
void CharacterEditor::setTopologyTextureCoordinates(std::size_t mesh,std::span<const TopologyUvEdit> edits)
{
    editor_.get().setTopologyTextureCoordinates(mesh,edits);
}
void CharacterEditor::setTextureCoordinates(std::size_t mesh,std::size_t set,std::span<const TopologyUvEdit> edits)
{editor_.get().setTextureCoordinates(mesh,set,edits);}
void CharacterEditor::sculpt(std::size_t mesh, Vector3 center, double radius, Vector3 displacement,
                             bool symmetry)
{
    editor_.get().sculpt(mesh, center, radius, displacement, symmetry);
}
BrushStrokeResult CharacterEditor::sculptStroke(std::size_t mesh,std::span<const SculptStrokeSample> samples,const MeshBrushSettings &settings,std::optional<BrushMirrorPlane> symmetry)
{return editor_.get().sculptStroke(mesh,samples,settings,symmetry);}
BrushStrokeResult CharacterEditor::paintWeights(std::size_t mesh,BoneIndex bone,std::span<const MeshBrushSample> samples,const SkinPaintOptions &options)
{return editor_.get().paintWeights(mesh,bone,samples,options);}
void CharacterEditor::smooth(std::size_t mesh, double strength, unsigned iterations)
{
    editor_.get().smooth(mesh, strength, iterations);
}
void CharacterEditor::setMorph(std::size_t mesh, MorphTarget morph)
{
    editor_.get().setMorph(mesh, std::move(morph));
}
void CharacterEditor::createMorph(std::size_t mesh, std::string name, std::span<const Vector3> target)
{
    editor_.get().createMorph(mesh, std::move(name), target);
}
void CharacterEditor::bakeMorphs(std::size_t mesh, std::span<const MorphValue> values)
{
    editor_.get().bakeMorphs(mesh, values);
}
void CharacterEditor::setWeights(std::size_t mesh, std::span<const VertexWeights> edits)
{
    editor_.get().setWeights(mesh, edits);
}
void CharacterEditor::smoothWeights(std::size_t mesh, double strength, unsigned iterations,
                                    std::size_t maxInfluences)
{
    editor_.get().smoothWeights(mesh, strength, iterations, maxInfluences);
}
void CharacterEditor::moveJoint(BoneIndex bone, Vector3 position, bool preserveChildren)
{
    editor_.get().moveJoint(bone, position, preserveChildren);
}
void CharacterEditor::renameJoint(BoneIndex bone, std::string name)
{
    editor_.get().renameJoint(bone, std::move(name));
}
bool CharacterEditor::renameJoints(std::span<const JointRename> names)
{
    return editor_.get().renameJoints(names);
}
bool CharacterEditor::setJointTransform(BoneIndex bone,Transform value,JointTransformSpace space,bool preserveChildren)
{return editor_.get().setJointTransform(bone,value,space,preserveChildren);}
bool CharacterEditor::reparentJoint(BoneIndex bone,BoneIndex parent,bool preserveWorld)
{return editor_.get().reparentJoint(bone,parent,preserveWorld);}
bool CharacterEditor::orientJoint(BoneIndex bone,Vector3 target,Vector3 up,bool preserveChildren)
{return editor_.get().orientJoint(bone,target,up,preserveChildren);}
bool CharacterEditor::undo()
{
    return editor_.get().undo();
}
bool CharacterEditor::redo()
{
    return editor_.get().redo();
}
} // namespace iiActorObject::NSCharacter
