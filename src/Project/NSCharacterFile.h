#pragma once

#include <iiActorObject/Export.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace iiActorObject
{
struct NSCharacterFileLimits final
{
    std::size_t maximumActorBytes = 256 * 1024 * 1024;
    std::size_t maximumSheetBytes = 4 * 1024 * 1024;
};

// Borrowed sections. Keep the source buffer alive and unmodified while using them.
struct NSCharacterFileView final
{
    std::span<const std::uint8_t> actorProject;
    std::string_view sheetJson;
};

// actorProject is a native .ii3dproject byte stream; sheetJson is a strict UTF-8 JSON object.
// Writes a version 1 envelope without re-encoding either section or performing file I/O.
[[nodiscard]] IIACTOROBJECT_EXPORT std::vector<std::uint8_t> packNSCharacterFile(
    std::span<const std::uint8_t> actorProject, std::string_view sheetJson,
    NSCharacterFileLimits limits = {});

// Validates the envelope, actor framing and sheet. importObject3DProject validates
// and restores the actor separately, including its project schema and checksum.
[[nodiscard]] IIACTOROBJECT_EXPORT NSCharacterFileView unpackNSCharacterFile(
    std::span<const std::uint8_t> bytes, NSCharacterFileLimits limits = {});
} // namespace iiActorObject
