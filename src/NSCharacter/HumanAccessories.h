#pragma once
#include <iiActorObject/NSCharacter/CharacterTemplateCore.h>
#include <filesystem>
#include <string>
#include <vector>

namespace iiActorObject::NSCharacter
{
struct HumanAccessoryInfo final
{
    std::string id,name,category;
};
struct HumanAccessorySelection final
{
    std::filesystem::path directory;
    std::vector<std::string> componentIds;
    bool hideCoveredBody=true;
};
// Reads the catalogue only; meshes, images and engine objects are not retained.
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<HumanAccessoryInfo> listMakeHumanAccessories(
    const std::filesystem::path &directory,std::size_t maximumBytes=1024*1024);
// Adds and removes accessories, including mixed changes, in a same-engine
// template. Retained slots keep their relative order.
// The core engine retains ownership; incompatible source layouts are rejected.
[[nodiscard]] IIACTOROBJECT_EXPORT bool setHumanAccessories(CharacterEditor &editor,
    const CharacterTemplate &alternative,const std::filesystem::path &directory,
    std::size_t maximumCatalogueBytes=1024*1024);
// Explicitly permit dropping edits on removed accessories in the new state.
// Undo retains those edits in the preceding core-owned history snapshot.
[[nodiscard]] IIACTOROBJECT_EXPORT bool setHumanAccessories(CharacterEditor &editor,
    const CharacterTemplate &alternative,const std::filesystem::path &directory,
    const TemplateSurfaceRemovalOptions &removal,std::size_t maximumCatalogueBytes=1024*1024);
} // namespace iiActorObject::NSCharacter
