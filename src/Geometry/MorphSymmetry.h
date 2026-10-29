#pragma once
#include <iiActorObject/MeshSymmetry.h>

namespace iiActorObject
{
struct MorphMirrorRequest final
{
    std::string source,target;
    std::uint32_t sourceTarget=NoTopologyIndex;
};
struct MorphMirrorOptions final
{
    bool replaceExisting=false;
    bool preserveExplicitNormals=true;
    std::span<const double> mask;
    double epsilon=1e-12;
    std::size_t maximumPointVisits=128'000'000;
    std::size_t maximumMorphDeltas=16'000'000;
    std::size_t maximumLayouts=4'000'000;
};
struct MorphMirrorResult final
{
    std::size_t created=0,replaced=0,positionDeltas=0,normalDeltas=0,unmappedSourceDeltas=0;
    [[nodiscard]] bool changed() const noexcept {return created!=0||replaced!=0;}
};
// For same-name replacement: one-sided copy or averaging, including centre-plane projection.
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<double> makeMorphSymmetryMask(
    const MeshSymmetryMap &mapping,MeshSymmetryDirection direction,double strength=1);
// Reads every source from the original snapshot, including simultaneous A<->B requests.
// Reflects displacement vectors; it does not erase intentional base-shape asymmetry.
// Explicit layout normals follow mirrored polygon corners, independently of UV seams.
[[nodiscard]] IIACTOROBJECT_EXPORT MorphMirrorResult mirrorMorphs(Object3DMesh &mesh,
    const MeshSymmetryMap &mapping,std::span<const MorphMirrorRequest> requests,
    const MorphMirrorOptions &options={});
} // namespace iiActorObject
