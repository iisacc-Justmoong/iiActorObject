#pragma once
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#include <filesystem>

namespace iiActorObject::NSCharacter
{
// Catalogue labels are source-asset names, not inferred personal attributes.
struct HumanSkinPreset final
{
    std::string id, label;
};
struct HumanSkinSelection final
{
    std::filesystem::path directory;
    std::string preset;
    std::size_t mesh = 0;
    std::size_t maximumBytes = 16 * 1024 * 1024;
};
// Reads catalogue metadata only; no image or engine ownership is retained.
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<HumanSkinPreset> listHumanSkinPresets(
    const std::filesystem::path &directory,std::size_t maximumBytes=1024*1024);
// Applies the selected skin to the mesh's material as one undoable edit.
// Returns false if the requested material and image are already installed.
[[nodiscard]] IIACTOROBJECT_EXPORT bool setHumanSkin(
    CharacterEditor &editor,const HumanSkinSelection &selection);
} // namespace iiActorObject::NSCharacter
