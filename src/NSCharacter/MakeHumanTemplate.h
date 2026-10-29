#pragma once
#include <iiActorObject/NSCharacter/HumanAccessories.h>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace iiActorObject::NSCharacter
{
struct HumanSkinSelection;
struct MakeHumanTemplateOptions final
{
    double scale=.1; // Original CC0 hm08 asset units -> metres.
    bool groundAtFeet=true;
    std::vector<std::string> visibleGroups{"body"};
    std::size_t maximumBytes=256*1024*1024;
    bool semanticControls=true;
    std::filesystem::path faceAssets; // Optional CC0 descriptor/image bundle.
    std::filesystem::path macroAssets; // Optional CC0 full morphology tensor bundle.
    std::filesystem::path expressionAssets; // Optional CC0 59-unit face pose program.
    std::optional<HumanAccessorySelection> accessories; // Optional authored hm08 accessories.
};
// Reads CC0 data only; decoding is transient, model/program ownership goes to the core.
[[nodiscard]] IIACTOROBJECT_EXPORT CharacterTemplate loadMakeHumanTemplate(
    const Object3DReference &engine,const std::filesystem::path &directory,
    const MakeHumanTemplateOptions &options={});
[[nodiscard]] IIACTOROBJECT_EXPORT CharacterTemplate loadMakeHumanTemplate(
    const Object3DReference &engine,const std::filesystem::path &directory,
    const MakeHumanTemplateOptions &options,const HumanSkinSelection &skin);
} // namespace iiActorObject::NSCharacter
