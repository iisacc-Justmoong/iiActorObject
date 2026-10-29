#pragma once
#include <iiActorObject/NSCharacter/CharacterAsset.h>
#include <iiActorObject/Object3DEditor.h>
#include <unordered_map>

namespace iiActorObject::NSCharacter
{
using VertexMove = iiActorObject::VertexMove;
using VertexWeights = iiActorObject::VertexWeights;
using MorphValue = iiActorObject::MorphValue;

// One host thread owns edits. Readers retain immutable snapshots across undo/redo.
class IIACTOROBJECT_EXPORT CharacterEditor final
{
  public:
    explicit CharacterEditor(std::shared_ptr<const CharacterAsset> asset, std::size_t historyLimit = 32);
    explicit CharacterEditor(Object3DEditorHandle editor);
    [[nodiscard]] const Object3DEditorHandle &session() const noexcept;
    [[nodiscard]] const std::shared_ptr<const CharacterAsset> &asset() const;
    [[nodiscard]] std::uint64_t revision() const;
    [[nodiscard]] std::string_view editLabel() const;
    [[nodiscard]] bool layered() const;
    [[nodiscard]] std::span<const TemplateValue> templateValues() const;
    [[nodiscard]] std::vector<Object3DEditLayerInfo> layers() const;
    void setTemplateValues(std::span<const TemplateValue> values);
    void setLayerEnabled(Object3DLayerId id,bool enabled);
    void setLayerWeight(Object3DLayerId id,double weight);
    [[nodiscard]] std::span<const MeshPointMask> layerPointMasks(Object3DLayerId id) const;
    void setLayerPointMasks(Object3DLayerId id,std::span<const MeshPointMask> masks);
    [[nodiscard]] std::vector<std::string> pointSelectionNames() const;
    [[nodiscard]] std::span<const MeshPointMask> pointSelection(std::string_view name) const;
    void setPointSelection(std::string name,std::span<const MeshPointMask> masks);
    void renamePointSelection(std::string_view oldName,std::string newName);
    void removePointSelection(std::string_view name);
    void setLayerPointSelection(Object3DLayerId id,std::string_view name);
    void renameLayer(Object3DLayerId id,std::string label);
    void moveLayer(Object3DLayerId id,std::size_t index);
    void removeLayer(Object3DLayerId id);
    void detachTemplate();
    [[nodiscard]] SurfaceAttachmentResult attachSurface(CharacterMesh surface,std::size_t sourceMesh,
        const SurfaceAttachmentOptions &options={});
    [[nodiscard]] std::span<const SurfaceFittingRule> surfaceFittings() const;
    void setSurfaceFittings(std::span<const SurfaceFittingRule> rules);
    // Bulk custom tools use the same validation and history boundary as built-in edits.
    void replace(CharacterData data, std::string label = "Replace character");
    [[nodiscard]] Object3DSelectionTransferReport replaceWithProjectedSelections(
        const Object3DHandle &target,std::span<const MeshMaskProjectionPair> pairs,
        const Object3DSelectionTransferOptions &options={},std::string label="Replace and transfer selections");
    void setReferencePose(std::optional<PoseReference> reference);
    [[nodiscard]] bool setMaterialSurface(std::size_t material,Object3DMaterialSurface surface);
    [[nodiscard]] Object3DExternalReport applyExternalEdits(const CharacterAsset &source,
        Object3DExternalSnapshot baseline,Object3DExternalSnapshot edited,const Object3DExternalOptions &options);
    void moveVertices(std::size_t mesh, std::span<const VertexMove> moves);
    void moveControlPoints(std::size_t mesh, std::span<const TopologyPointMove> moves);
    void setControlWeights(std::size_t mesh, std::span<const TopologyPointWeights> edits);
    void setSkinningProfile(std::size_t mesh, SkinningProfile profile);
    [[nodiscard]] SurfaceBindResult bindSurfaceWeights(std::size_t mesh,std::span<const BoneEnvelope> envelopes,
        const SurfaceBindOptions &options={});
    [[nodiscard]] SkeletonSurfaceBindResult bindSurfaceWeightsFromSkeleton(std::size_t mesh,
        const SkeletonEnvelopeOptions &inference={},const SurfaceBindOptions &binding={});
    [[nodiscard]] SkinTransferResult transferWeights(std::size_t mesh,const CharacterAsset &source,std::size_t sourceMesh,const SkinTransferOptions &options={});
    [[nodiscard]] SkinTransferResult transferWeights(std::size_t mesh,const CharacterAsset &source,std::size_t sourceMesh,
        const SkinTransferOptions &options,SkinningProfileTransfer profile);
    [[nodiscard]] SkinNormalTransferResult transferWeightsWithNormalTies(std::size_t mesh,const CharacterAsset &source,
        std::size_t sourceMesh,const SkinTransferOptions &options,SkinningProfileTransfer profile,
        const SkinNormalTransferOptions &normals,SurfaceQueryBudget &budget);
    [[nodiscard]] SkinTransferResult mirrorWeights(std::size_t mesh,std::span<const BoneIndex> boneMirrors,const SkinMirrorOptions &options={});
    [[nodiscard]] SkinTransferResult mirrorWeights(std::size_t mesh,std::span<const BoneIndex> boneMirrors,
        const SkinMirrorOptions &options,SkinningProfileTransfer profile);
    void setControlMorph(std::size_t mesh, TopologyMorph morph);
    [[nodiscard]] MeshSymmetryResult symmetrizeMesh(std::size_t mesh,const MeshSymmetryMap &mapping,
        const MeshSymmetryEditOptions &options={});
    [[nodiscard]] MorphMirrorResult mirrorMorphs(std::size_t mesh,const MeshSymmetryMap &mapping,
        std::span<const MorphMirrorRequest> requests,const MorphMirrorOptions &options={});
    [[nodiscard]] SurfaceFitResult fitSurface(std::size_t mesh,std::span<const SurfaceCollider *const> obstacles,
        const SurfaceFitOptions &options={});
    [[nodiscard]] SurfaceFitResult fitSurface(std::size_t mesh,const SurfaceCollider &obstacle,
        const SurfaceFitOptions &options={});
    void setTopologyTextureCoordinates(std::size_t mesh, std::span<const TopologyUvEdit> edits);
    void setTextureCoordinates(std::size_t mesh,std::size_t set,std::span<const TopologyUvEdit> edits);
    void sculpt(std::size_t mesh, Vector3 center, double radius, Vector3 displacement,
                bool symmetry = false);
    [[nodiscard]] BrushStrokeResult sculptStroke(std::size_t mesh,std::span<const SculptStrokeSample> samples,
        const MeshBrushSettings &settings={},std::optional<BrushMirrorPlane> symmetry={});
    [[nodiscard]] BrushStrokeResult paintWeights(std::size_t mesh,BoneIndex bone,std::span<const MeshBrushSample> samples,
        const SkinPaintOptions &options={});
    void smooth(std::size_t mesh, double strength, unsigned iterations = 1);
    void setMorph(std::size_t mesh, MorphTarget morph);
    void setDefaultMorphWeight(std::size_t mesh, std::string_view name, double value);
    void createMorph(std::size_t mesh, std::string name, std::span<const Vector3> target);
    void bakeMorphs(std::size_t mesh, std::span<const MorphValue> values);
    void setWeights(std::size_t mesh, std::span<const VertexWeights> edits);
    void smoothWeights(std::size_t mesh, double strength, unsigned iterations = 1,
                       std::size_t maxInfluences = 4);
    // Sets an object-space joint position. Optionally retains direct children's world positions.
    void moveJoint(BoneIndex bone, Vector3 position, bool preserveChildren = true);
    [[nodiscard]] bool setJointTransform(BoneIndex bone,Transform value,
        JointTransformSpace space=JointTransformSpace::Local,bool preserveChildren=true);
    [[nodiscard]] bool reparentJoint(BoneIndex bone,BoneIndex parent,bool preserveWorld=true);
    [[nodiscard]] bool orientJoint(BoneIndex bone,Vector3 target,Vector3 up={0,0,1},bool preserveChildren=true);
    void renameJoint(BoneIndex bone, std::string name);
    [[nodiscard]] bool renameJoints(std::span<const JointRename> names);
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();

  private:
    Object3DEditorHandle editor_;
    mutable std::shared_ptr<const CharacterAsset> asset_;
    mutable std::unordered_map<Object3DId, std::weak_ptr<const CharacterAsset>> views_;
};
} // namespace iiActorObject::NSCharacter
