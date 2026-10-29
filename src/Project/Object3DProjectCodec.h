#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace iiActorObject
{
struct Object3DData;
struct Object3DProjectOptions;
namespace object3d_detail
{
struct ProjectArchive;
// Serializes only an in-memory project graph; engine publication is separate.
void validateProjectLimits(const Object3DProjectOptions &options);
void assertEquivalentProjectAssets(Object3DData &expected,Object3DData &stored,const Object3DProjectOptions &options);
[[nodiscard]] std::vector<std::uint8_t> encodeProjectArchive(ProjectArchive &graph,const Object3DProjectOptions &options);
[[nodiscard]] ProjectArchive decodeProjectArchive(std::span<const std::uint8_t> bytes,const Object3DProjectOptions &options);
}
}
