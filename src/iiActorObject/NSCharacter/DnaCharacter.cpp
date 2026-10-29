#include "DnaCharacter.h"
#include <stdexcept>
namespace iiActorObject::NSCharacter
{
Object3DDnaExport exportDna(const DnaCharacter &character,
    std::span<const Object3DRigHandle> additional,DnaExportOptions options)
{
    if (!character.asset||character.asset->object()!=character.rig.get().object())
        throw std::invalid_argument("Character view and DNA rig must reference the same core object");
    return exportObject3DDna(character.rig,additional,options);
}
DnaCharacter importDna(Object3DEngine &engine, std::span<const std::byte> bytes, DnaImportOptions options)
{
    return importDna(engine.reference(), bytes, options);
}
DnaCharacter importDna(const Object3DReference &engine, std::span<const std::byte> bytes, DnaImportOptions options)
{
    auto imported = importObject3DDna(engine, bytes, options);
    return {std::make_shared<CharacterAsset>(std::move(imported.object)),
            std::move(imported.rig), std::move(imported.warnings)};
}
} // namespace iiActorObject::NSCharacter
