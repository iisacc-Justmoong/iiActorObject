#pragma once
#include <iiActorObject/Object3DAsset.h>

namespace iiActorObject
{
struct MeshPointMask final
{
    std::size_t mesh=0;
    std::vector<double> weights;
    bool operator==(const MeshPointMask&)const=default;
};
enum class MeshMaskCombine {Union,Intersect,Subtract,Multiply};
enum class MeshBrushFalloff {Constant,Linear,Smooth,Sharp};
enum class MeshBrushDistance {Euclidean,Surface};
struct MeshBrushSample final {Vector3 center;double radius=.05,strength=1;};
struct MeshBrushSettings final
{
    MeshBrushFalloff falloff=MeshBrushFalloff::Smooth;
    MeshBrushDistance distance=MeshBrushDistance::Euclidean;
    std::span<const double> mask;
    bool includeUnusedPoints=false;
    std::size_t maximumPointVisits=128'000'000;
};
struct BrushStrokeResult final {std::size_t touchedPoints=0,changedPoints=0;};
struct SculptStrokeSample final {MeshBrushSample brush;Vector3 displacement;};
struct BrushMirrorPlane final {Vector3 normal{1,0,0};double offset=0;};
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<double> makeMeshPointMask(const Object3DMesh &mesh,
    std::span<const std::uint32_t> points,double value=1);
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<double> makeMeshBrushMask(const Object3DMesh &mesh,
    MeshBrushSample sample,const MeshBrushSettings &settings={});
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<double> growMeshPointMask(const Object3DMesh &mesh,
    std::span<const double> mask,unsigned steps,double decay=1);
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<double> smoothMeshPointMask(const Object3DMesh &mesh,
    std::span<const double> mask,double strength,unsigned iterations=1);
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<double> invertMeshPointMask(std::span<const double> mask);
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<double> combineMeshPointMasks(std::span<const double> a,
    std::span<const double> b,MeshMaskCombine operation);
IIACTOROBJECT_EXPORT void validateMeshPointMasks(const Object3DData &data,std::span<const MeshPointMask> masks);
class IIACTOROBJECT_EXPORT MeshSculptor final
{
  public:
    // Distances and neighbourhoods use the immutable stroke-start geometry.
    [[nodiscard]] static BrushStrokeResult stroke(Object3DData &data,std::size_t mesh,
        std::span<const SculptStrokeSample> samples,const MeshBrushSettings &settings={},
        std::optional<BrushMirrorPlane> symmetry={});
};
} // namespace iiActorObject
