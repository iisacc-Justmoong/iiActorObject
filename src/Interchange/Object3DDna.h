#pragma once
#include <iiActorObject/Object3DRig.h>

namespace iiActorObject
{
struct DnaImportOptions final
{
    std::size_t geometryLod = 0;
    std::size_t maximumBytes = 256 * 1024 * 1024;
    std::size_t maximumMemory = 1024ULL * 1024 * 1024;
    std::size_t maximumVertices = 2'000'000;
    std::size_t maximumMorphDeltas = 16'000'000;
    std::size_t maximumJoints = 6553;
    std::size_t maximumMeshes = 512;
};
struct Object3DDnaImport final
{
    Object3DHandle object;
    Object3DRigHandle rig;
    std::vector<std::string> warnings;
};
using DnaExportOptions = RigSourceExportOptions;
using Object3DDnaExport = RigSourceExportResult;
[[nodiscard]] IIACTOROBJECT_EXPORT bool object3DDnaAvailable() noexcept;
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DDnaImport importObject3DDna(
    const Object3DReference &engine, std::span<const std::byte> bytes, DnaImportOptions options = {});
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DDnaExport exportObject3DDna(
    const Object3DRigHandle &rig, std::span<const Object3DRigHandle> additional = {}, DnaExportOptions options = {});
} // namespace iiActorObject
