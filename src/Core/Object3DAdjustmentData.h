#pragma once
#include "Object3DData.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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
} // namespace iiActorObject::object3d_detail
