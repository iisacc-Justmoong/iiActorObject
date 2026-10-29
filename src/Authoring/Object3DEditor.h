#pragma once
#include <iiActorObject/Object3DEditLayerData.h>
#include <iiActorObject/SurfaceFittingRules.h>
#include <iiActorObject/Object3DEngine.h>
#include <iiActorObject/Object3DTemplate.h>
#include <iiActorObject/SurfaceAttachment.h>
#include <iiActorObject/SkinBinding.h>
#include <iiActorObject/SkinPainting.h>
#include <iiActorObject/SkeletonEditing.h>
#include <iiActorObject/Object3DExternalEdits.h>
#include <iiActorObject/MorphSymmetry.h>
#include <iiActorObject/SurfaceFitting.h>
#include <iiActorObject/MeshMaskProjectionAsset.h>

namespace iiActorObject
{
namespace object3d_detail { struct LayerState; struct PointSelectionState; }
struct TemplateValue;
struct VertexMove final
{
    std::size_t vertex = 0;
    Vector3 displacement;
};
struct VertexWeights final
{
    std::size_t vertex = 0;
    std::vector<SkinWeight> weights;
};
struct Object3DSelectionTransferOptions final
{
    MeshMaskProjectionOptions projection;
    // Detaching retains the old template and its layers in undo history.
    bool detachTemplate=false;
    // By default every source mesh containing a nonzero selection needs a pair.
    bool discardUnmappedSelections=false;
};
struct Object3DSelectionTransferReport final
{
    std::size_t selectionSets=0,discardedSourceRows=0,projectedPoints=0,skippedUnusedPoints=0;
    std::size_t nodeVisits=0,triangleTests=0,weightEvaluations=0,outputWeights=0;
    double maximumDistance=0;
};
struct TemplateSurfaceRemovalOptions final
{
    // Removed edits remain in undo history; the active state drops only rows
    // belonging to removed meshes. Named selections keep their names.
    bool discardRemovedEdits=false;
};
class IIACTOROBJECT_EXPORT Object3DEditor final
{
  public:
    Object3DEditor(const Object3DEditor &) = delete;
    Object3DEditor &operator=(const Object3DEditor &) = delete;
    Object3DEditor(Object3DEditor &&) = delete;
    Object3DEditor &operator=(Object3DEditor &&) = delete;
    [[nodiscard]] const Object3DHandle &object() const noexcept;
    [[nodiscard]] std::uint64_t revision() const noexcept;
    [[nodiscard]] std::string_view editLabel() const noexcept;
    [[nodiscard]] bool layered() const noexcept;
    [[nodiscard]] std::span<const TemplateValue> templateValues() const noexcept;
    [[nodiscard]] Object3DTemplateHandle templateProgram() const;
    [[nodiscard]] std::vector<Object3DEditLayerInfo> layers() const;
    void setTemplateValues(std::span<const TemplateValue> values);
    void setLayerEnabled(Object3DLayerId id,bool enabled);
    void setLayerWeight(Object3DLayerId id,double weight);
    [[nodiscard]] std::span<const MeshPointMask> layerPointMasks(Object3DLayerId id) const;
    void setLayerPointMasks(Object3DLayerId id,std::span<const MeshPointMask> masks);
    // Named, saved point masks. Missing mesh rows mean no points selected.
    // Returned spans expire when the editor state changes or its engine dies.
    [[nodiscard]] std::vector<std::string> pointSelectionNames() const;
    [[nodiscard]] std::span<const MeshPointMask> pointSelection(std::string_view name) const;
    void setPointSelection(std::string name,std::span<const MeshPointMask> masks);
    void renamePointSelection(std::string_view oldName,std::string newName);
    void removePointSelection(std::string_view name);
    // Replace a template layer's masks; meshes absent from the selection receive zero weight.
    void setLayerPointSelection(Object3DLayerId id,std::string_view name);
    void renameLayer(Object3DLayerId id,std::string label);
    void moveLayer(Object3DLayerId id,std::size_t index);
    void removeLayer(Object3DLayerId id);
    // Keeps the current result as an ordinary editable snapshot; undo restores layers.
    void detachTemplate();
    [[nodiscard]] SurfaceAttachmentResult attachSurface(Object3DMesh surface,std::size_t sourceMesh,
        const SurfaceAttachmentOptions &options={});
    // Atomically replace existing follower slots from a same-engine template.
    // The listed visibility meshes keep their source IDs and take the other
    // template's hidden-face layout. Existing edits on replaced slots are rejected.
    // Grounded visibility changes preserve the original support point IDs.
    [[nodiscard]] bool replaceTemplateSurfaces(const Object3DTemplateHandle &alternative,
        std::span<const std::size_t> meshes,std::span<const std::size_t> visibilityMeshes={});
    // Remove follower slots and remap all retained mesh references atomically.
    // The alternative supplies retained slots in their original order and the
    // selected visibility layouts, and must belong to the same engine.
    [[nodiscard]] bool removeTemplateSurfaces(const Object3DTemplateHandle &alternative,
        std::span<const std::size_t> meshes,std::span<const std::size_t> visibilityMeshes={},
        const TemplateSurfaceRemovalOptions &options={});
    // Insert follower slots from the alternative template. Added slot IDs refer
    // to that template; visibility mesh IDs refer to the current template.
    [[nodiscard]] bool addTemplateSurfaces(const Object3DTemplateHandle &alternative,
        std::span<const std::size_t> meshes,std::span<const std::size_t> visibilityMeshes={});
    // Remove and insert follower slots as one history step. Removed and
    // visibility IDs refer to the current template; added IDs refer to the
    // alternative. Retained slots keep their relative order.
    [[nodiscard]] bool changeTemplateSurfaces(const Object3DTemplateHandle &alternative,
        std::span<const std::size_t> removedMeshes,std::span<const std::size_t> addedMeshes,
        std::span<const std::size_t> visibilityMeshes={},const TemplateSurfaceRemovalOptions &options={});
    [[nodiscard]] std::span<const SurfaceFittingRule> surfaceFittings() const;
    void setSurfaceFittings(std::span<const SurfaceFittingRule> rules);
    void replace(Object3DData data, std::string label = "Replace 3D object");
    // Target must be owned by the same engine. Mesh and all named selections
    // change in one history step; the input assets are not mutated.
    [[nodiscard]] Object3DSelectionTransferReport replaceWithProjectedSelections(
        const Object3DHandle &target,std::span<const MeshMaskProjectionPair> pairs,
        const Object3DSelectionTransferOptions &options={},std::string label="Replace and transfer selections");
    void setReferencePose(std::optional<PoseReference> reference);
    // One undoable material change; mode and sidedness are discrete layer properties.
    [[nodiscard]] bool setMaterialSurface(std::size_t material,Object3DMaterialSurface surface);
    [[nodiscard]] Object3DExternalReport applyExternalEdits(const Object3DAsset &source,
        Object3DExternalSnapshot baseline,Object3DExternalSnapshot edited,const Object3DExternalOptions &options);
    void moveVertices(std::size_t mesh, std::span<const VertexMove> moves);
    void moveControlPoints(std::size_t mesh, std::span<const TopologyPointMove> moves);
    void setControlWeights(std::size_t mesh, std::span<const TopologyPointWeights> edits);
    // Source-point order when topology exists, otherwise render-vertex order.
    void setSkinningProfile(std::size_t mesh, SkinningProfile profile);
    [[nodiscard]] SurfaceBindResult bindSurfaceWeights(std::size_t mesh,std::span<const BoneEnvelope> envelopes,
        const SurfaceBindOptions &options={});
    [[nodiscard]] SkeletonSurfaceBindResult bindSurfaceWeightsFromSkeleton(std::size_t mesh,
        const SkeletonEnvelopeOptions &inference={},const SurfaceBindOptions &binding={});
    [[nodiscard]] SkinTransferResult transferWeights(std::size_t mesh,const Object3DAsset &source,std::size_t sourceMesh,const SkinTransferOptions &options={});
    [[nodiscard]] SkinTransferResult transferWeights(std::size_t mesh,const Object3DAsset &source,std::size_t sourceMesh,
        const SkinTransferOptions &options,SkinningProfileTransfer profile);
    // Records an undoable change only when the resulting skin differs.
    [[nodiscard]] SkinNormalTransferResult transferWeightsWithNormalTies(std::size_t mesh,const Object3DAsset &source,
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
    // Set 0 is primary; later sets use their source-coordinate IDs (or render-vertex IDs without topology).
    void setTextureCoordinates(std::size_t mesh,std::size_t set,std::span<const TopologyUvEdit> edits);
    void sculpt(std::size_t mesh, Vector3 center, double radius, Vector3 displacement,
                bool symmetry = false);
    [[nodiscard]] BrushStrokeResult sculptStroke(std::size_t mesh,std::span<const SculptStrokeSample> samples,
        const MeshBrushSettings &settings={},std::optional<BrushMirrorPlane> symmetry={});
    [[nodiscard]] BrushStrokeResult paintWeights(std::size_t mesh,BoneIndex bone,std::span<const MeshBrushSample> samples,
        const SkinPaintOptions &options={});
    void smooth(std::size_t mesh, double strength, unsigned iterations = 1);
    void setMorph(std::size_t mesh, MorphTarget morph);
    // Sets the default for newly created instances; zero removes the override.
    void setDefaultMorphWeight(std::size_t mesh, std::string_view name, double value);
    void createMorph(std::size_t mesh, std::string name, std::span<const Vector3> target);
    void bakeMorphs(std::size_t mesh, std::span<const MorphValue> values);
    void setWeights(std::size_t mesh, std::span<const VertexWeights> edits);
    void smoothWeights(std::size_t mesh, double strength, unsigned iterations = 1,
                       std::size_t maxInfluences = 4);
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
    friend class Object3DReference;
    friend struct object3d_detail::ProjectAccess;
    Object3DEditor(Object3DReference owner, Object3DHandle object, std::size_t historyLimit);
    Object3DEditor(Object3DReference owner,const Object3DTemplateHandle &program,
        std::span<const TemplateValue> values,std::size_t historyLimit);
    struct Entry final
    {
        Object3DHandle object;
        std::string label;
        std::shared_ptr<const object3d_detail::LayerState> layers;
        std::shared_ptr<const object3d_detail::PointSelectionState> selections;
        std::shared_ptr<const std::vector<std::optional<std::size_t>>> selectionRemap;
    };
    void commit(Object3DData data, std::string label);
    void publish(Object3DData data,std::string label,std::shared_ptr<const object3d_detail::LayerState> layers);
    void publishSelections(std::shared_ptr<const object3d_detail::PointSelectionState> selections,std::string label);
    [[nodiscard]] Object3DData rebuild(const object3d_detail::LayerState &state) const;
    [[nodiscard]] Object3DData applyLayers(Object3DData data,const object3d_detail::LayerState &state) const;
    [[nodiscard]] std::shared_ptr<object3d_detail::LayerState> copyLayers() const;
    void checkRevision() const;
    Object3DReference owner_;
    std::vector<Entry> history_;
    std::size_t cursor_ = 0, limit_ = 32;
    std::uint64_t revision_ = 1;
    Object3DLayerId nextLayer_=1;
};
} // namespace iiActorObject
