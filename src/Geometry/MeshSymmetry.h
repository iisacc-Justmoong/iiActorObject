#pragma once
#include <iiActorObject/MeshEditing.h>

namespace iiActorObject
{
enum class MeshSymmetryUnmatched { Reject, Keep };
struct MeshSymmetryOptions final
{
    BrushMirrorPlane plane;
    double maximumDistance=1e-5;
    double centreTolerance=1e-7;
    bool includeUnusedPoints=true;
    MeshSymmetryUnmatched unmatched=MeshSymmetryUnmatched::Reject;
    std::size_t maximumPoints=4'000'000;
    std::size_t maximumPointVisits=128'000'000;
};
// Stable source-point correspondence. Does not own an engine, asset or execution session.
// It can be reused after shape/UV edits, but not after point-ID/connectivity changes.
class IIACTOROBJECT_EXPORT MeshSymmetryMap final
{
  public:
    [[nodiscard]] static MeshSymmetryMap create(const Object3DMesh &mesh,const MeshSymmetryOptions &options={});
    // Explicit pairs resolve coincident/ambiguous geometry; search distance is diagnostic only.
    [[nodiscard]] static MeshSymmetryMap fromPairs(const Object3DMesh &mesh,
        std::span<const std::uint32_t> mirrors,const MeshSymmetryOptions &options={});
    [[nodiscard]] std::span<const std::uint32_t> mirrors() const noexcept;
    [[nodiscard]] std::span<const std::int8_t> sides() const noexcept;
    [[nodiscard]] const BrushMirrorPlane &plane() const noexcept;
    [[nodiscard]] std::size_t mappedPointCount() const noexcept;
    [[nodiscard]] double maximumMatchDistance() const noexcept;
    [[nodiscard]] std::size_t pointVisits() const noexcept;
    void validateFor(const Object3DMesh &mesh) const;
    [[nodiscard]] Vector3 reflectPoint(Vector3 point) const;
    [[nodiscard]] Vector3 reflectVector(Vector3 value) const;
  private:
    MeshSymmetryMap()=default;
    BrushMirrorPlane plane_;
    std::vector<std::uint32_t> mirrors_,connectivity_;
    std::vector<std::int8_t> sides_;
    std::size_t mappedPoints_=0,pointVisits_=0;
    double maximumMatchDistance_=0;
};
enum class MeshSymmetryDirection { NegativeToPositive, PositiveToNegative, Average };
struct MeshSymmetryEditOptions final
{
    MeshSymmetryDirection direction=MeshSymmetryDirection::Average;
    double strength=1;
    std::span<const double> mask;
    double epsilon=1e-12;
};
struct MeshSymmetryResult final
{
    std::size_t changedPoints=0,unmappedPoints=0;
};
// Changes positions only. Skin, morph identities, source IDs and UVs are preserved.
[[nodiscard]] IIACTOROBJECT_EXPORT MeshSymmetryResult symmetrizeMesh(Object3DMesh &mesh,
    const MeshSymmetryMap &mapping,const MeshSymmetryEditOptions &options={});
} // namespace iiActorObject
