#include "NSCharacterFile.h"

#include <iiJson/Json.h>

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace iiActorObject
{
namespace
{
using Bytes = std::span<const std::uint8_t>;
constexpr std::array<std::uint8_t, 8> signature{'N','S','C','H','A','R','\r','\n'};
constexpr std::uint32_t fileVersion = 1;
constexpr std::uint32_t nativeProjectEncoding = 1;
constexpr std::size_t headerSize = 36;
constexpr std::array<std::uint8_t, 8> nativeProjectSignature{'i','i','A','O','P','R','J',0};
constexpr std::size_t nativeProjectHeaderSize = 24;

Bytes asBytes(std::string_view text)
{
    return {reinterpret_cast<const std::uint8_t *>(text.data()), text.size()};
}

void appendLittleEndian(std::vector<std::uint8_t> &bytes, std::uint64_t value, unsigned width)
{
    for (unsigned i = 0; i < width; ++i)
        bytes.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}

std::uint64_t readLittleEndian(Bytes bytes, std::size_t offset, unsigned width)
{
    std::uint64_t value = 0;
    for (unsigned i = 0; i < width; ++i)
        value |= std::uint64_t(bytes[offset + i]) << (8 * i);
    return value;
}

std::uint32_t sheetChecksum(Bytes sheet)
{
    static constexpr auto table = [] {
        std::array<std::uint32_t, 256> result{};
        for (std::uint32_t i = 0; i < result.size(); ++i) {
            auto value = i;
            for (unsigned bit = 0; bit < 8; ++bit)
                value = (value >> 1) ^ ((value & 1u) ? 0xedb88320u : 0u);
            result[i] = value;
        }
        return result;
    }();
    std::uint32_t value = 0xffffffffu;
    for (const auto byte : sheet)
        value = (value >> 8) ^ table[(value ^ byte) & 0xffu];
    return ~value;
}

void checkSizes(std::size_t actor, std::size_t sheet, NSCharacterFileLimits limits)
{
    if (actor > limits.maximumActorBytes || sheet > limits.maximumSheetBytes)
        throw std::length_error(".nscharacter section byte limit exceeded");
}

void checkActor(Bytes actor)
{
    // This is the persisted project header, independent of the current writer's
    // project version. Opening the 3D asset still uses the core project decoder.
    if (actor.size() < nativeProjectHeaderSize ||
        !std::equal(nativeProjectSignature.begin(), nativeProjectSignature.end(), actor.begin()) ||
        readLittleEndian(actor, 8, 4) == 0 ||
        readLittleEndian(actor, 12, 8) != actor.size() - nativeProjectHeaderSize)
        throw std::invalid_argument("Invalid native .ii3dproject actor envelope");
}

void checkSheet(std::string_view sheet, NSCharacterFileLimits fileLimits)
{
    // Version 1 owns the validation profile rather than taking the parser's defaults.
    ii::json::Limits limits;
    limits.input_bytes = fileLimits.maximumSheetBytes;
    limits.string_bytes = 1024 * 1024;
    limits.items = 100'000;
    limits.depth = 64;
    const auto parsed = ii::json::parse(sheet, limits);
    if (!parsed || !parsed->get<ii::json::Value::Object>())
        throw std::invalid_argument(".nscharacter sheet must be a valid UTF-8 JSON object within format limits");
}
} // namespace

std::vector<std::uint8_t> packNSCharacterFile(Bytes actor, std::string_view sheet,
                                              NSCharacterFileLimits limits)
{
    checkSizes(actor.size(), sheet.size(), limits);
    if (actor.size() > std::numeric_limits<std::size_t>::max() - headerSize ||
        sheet.size() > std::numeric_limits<std::size_t>::max() - headerSize - actor.size())
        throw std::length_error(".nscharacter file size overflow");
    checkActor(actor);
    checkSheet(sheet, limits);

    const auto sheetBytes = asBytes(sheet);
    std::vector<std::uint8_t> bytes;
    bytes.reserve(headerSize + actor.size() + sheetBytes.size());
    bytes.insert(bytes.end(), signature.begin(), signature.end());
    appendLittleEndian(bytes, fileVersion, 4);
    appendLittleEndian(bytes, nativeProjectEncoding, 4);
    appendLittleEndian(bytes, actor.size(), 8);
    appendLittleEndian(bytes, sheetBytes.size(), 8);
    appendLittleEndian(bytes, sheetChecksum(sheetBytes), 4);
    bytes.insert(bytes.end(), actor.begin(), actor.end());
    bytes.insert(bytes.end(), sheetBytes.begin(), sheetBytes.end());
    return bytes;
}

NSCharacterFileView unpackNSCharacterFile(Bytes bytes, NSCharacterFileLimits limits)
{
    if (bytes.size() < headerSize || !std::equal(signature.begin(), signature.end(), bytes.begin()))
        throw std::invalid_argument("Invalid .nscharacter signature or incomplete header");
    if (readLittleEndian(bytes, 8, 4) != fileVersion)
        throw std::invalid_argument("Unsupported .nscharacter file version");
    if (readLittleEndian(bytes, 12, 4) != nativeProjectEncoding)
        throw std::invalid_argument("Unsupported .nscharacter actor encoding");

    const auto actorSize = readLittleEndian(bytes, 16, 8);
    const auto sheetSize = readLittleEndian(bytes, 24, 8);
    const auto payloadSize = bytes.size() - headerSize;
    if (actorSize > payloadSize || sheetSize != payloadSize - actorSize)
        throw std::invalid_argument("Inconsistent .nscharacter section lengths");
    checkSizes(static_cast<std::size_t>(actorSize), static_cast<std::size_t>(sheetSize), limits);

    const auto payload = bytes.subspan(headerSize);
    const auto actor = payload.first(static_cast<std::size_t>(actorSize));
    const auto sheetBytes = payload.subspan(actor.size());
    if (sheetChecksum(sheetBytes) != readLittleEndian(bytes, 32, 4))
        throw std::invalid_argument("Corrupt .nscharacter sheet checksum");
    checkActor(actor);
    const std::string_view sheet{reinterpret_cast<const char *>(sheetBytes.data()), sheetBytes.size()};
    checkSheet(sheet, limits);
    return {actor, sheet};
}
} // namespace iiActorObject
