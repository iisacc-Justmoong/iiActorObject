#pragma once
// Private core authoring representation. Not part of the installed API.
#include "Object3DEditor.h"
#include "Object3DTemplate.h"

namespace iiActorObject::object3d_detail
{
struct UvDisplacement final { std::uint32_t index;TextureCoordinate value; };
struct UvSetAdjustment final {std::size_t set=0;std::vector<UvDisplacement> values;};
struct NormalAdjustment final {std::uint32_t index=0;Quaternion rotation;};
struct MeshAdjustment final
{
    std::size_t mesh=0;
    std::vector<TopologyPointDelta> positions;
    std::vector<TopologyPointWeights> weights;
    std::vector<UvDisplacement> uv;
    std::optional<std::vector<TopologyMorph>> sourceMorphs;
    std::optional<std::vector<MorphTarget>> renderMorphs;
    std::optional<std::string> name;
    std::optional<std::size_t> material;
    std::optional<std::vector<std::size_t>> triangleMaterials;
    std::vector<UvSetAdjustment> additionalUv;
    std::vector<NormalAdjustment> normals;
    std::optional<SkinningProfile> skinning;
};
struct JointAdjustment final
{
    BoneIndex bone=NoBone;
    Vector3 position;
    Quaternion rotation;
    std::optional<std::string> name;
    Vector3 scale;
    std::optional<BoneIndex> parent;
};
struct Adjustment final
{
    std::vector<MeshAdjustment> meshes;
    std::vector<JointAdjustment> joints;
    std::optional<std::string> name;
    std::optional<std::vector<Object3DMaterial>> materials;
    std::optional<std::vector<Object3DImage>> images;
    std::optional<std::vector<Object3DJointSemantic>> semantics;
    [[nodiscard]] bool empty() const noexcept;
};
struct EditLayer final
{
    Object3DEditLayerInfo info;
    std::shared_ptr<const Adjustment> adjustment;
    std::shared_ptr<const std::vector<MeshPointMask>> pointMasks={};
};
struct LayerState final
{
    Object3DTemplateHandle program;
    std::vector<TemplateValue> values;
    std::vector<EditLayer> layers;
    std::optional<PoseReference> referencePose;
};
[[nodiscard]] Adjustment difference(const Object3DData &before,const Object3DData &after);
using NormalAdjustmentKey=std::pair<std::size_t,std::uint32_t>;
[[nodiscard]] std::vector<NormalAdjustmentKey> normalAdjustmentKeys(const Adjustment &adjustment);
[[nodiscard]] std::vector<NormalAdjustmentKey> changedNormals(const Object3DData &before,const Object3DData &after);
void clearNormalAdjustments(Adjustment &adjustment);
void setNormalAdjustments(Adjustment &adjustment,const Object3DData &predicted,const Object3DData &desired,
    std::span<const NormalAdjustmentKey> keys);
[[nodiscard]] Object3DLayerEvaluation layerEvaluationFor(const Object3DTemplateDefinition &definition) noexcept;
void applySurfaces(Object3DData &data,const Adjustment &adjustment,double weight,std::span<std::uint8_t> moved={},
    std::span<const MeshPointMask> masks={});
void applyJoints(Object3DData &data,const Adjustment &adjustment,double weight);
void refreshSurfaces(Object3DData &data,std::span<const std::uint8_t> moved={});
void applyNormalSurfaces(Object3DData &data,const Adjustment &adjustment,double weight,
    std::span<const MeshPointMask> masks={});
} // namespace iiActorObject::object3d_detail
