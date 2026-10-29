#pragma once
#include <iiActorObject/NSCharacter/CharacterAsset.h>
#include <iiActorObject/Object3DDna.h>

namespace iiActorObject::NSCharacter
{
// Both handles borrow resources owned by the caller's core Object3DEngine.
struct DnaCharacter final
{
    std::shared_ptr<const CharacterAsset> asset;
    Object3DRigHandle rig;
    std::vector<std::string> warnings;
};
[[nodiscard]] IIACTOROBJECT_EXPORT DnaCharacter importDna(Object3DEngine &engine,
    std::span<const std::byte> bytes, DnaImportOptions options = {});
[[nodiscard]] IIACTOROBJECT_EXPORT DnaCharacter importDna(const Object3DReference &engine,
    std::span<const std::byte> bytes, DnaImportOptions options = {});
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DDnaExport exportDna(const DnaCharacter &character,
    std::span<const Object3DRigHandle> additional = {}, DnaExportOptions options = {});
} // namespace iiActorObject::NSCharacter
