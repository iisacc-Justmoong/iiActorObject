#pragma once
#include <iiActorObject/Object3DAsset.h>

namespace iiActorObject
{
struct Object3DObjOptions final
{
    double scale=1;
    Vector3 translation;
    // Empty includes every face. Hidden groups retain source points and polygon IDs.
    std::vector<std::string> visibleGroups;
    std::size_t maximumBytes=64*1024*1024;
    std::size_t maximumPoints=2'000'000;
    std::size_t maximumCorners=8'000'000;
};
// Geometry-only OBJ; never resolves mtllib or external paths. Unknown records fail.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DData parseObject3DObj(
    std::string_view source,const Object3DObjOptions &options={});
// Generic sparse point offsets: zero-based point ID and three decimal displacements.
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<TopologyPointDelta> parseObject3DTarget(
    std::string_view source,std::size_t pointCount,double scale=1,
    std::size_t maximumBytes=64*1024*1024);
} // namespace iiActorObject
