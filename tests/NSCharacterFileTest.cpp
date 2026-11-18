#include <iiActorObject/NSCharacterFile.h>
#include <iiActorObject/Object3DProject.h>
#ifdef IIACTOROBJECT_TEST_REQUIRE_CORE_ONLY
#include <iiActorObject/Features.h>
static_assert(!IIACTOROBJECT_HAS_NSCHARACTER);
#endif

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace
{
using namespace iiActorObject;
constexpr std::u8string_view sheetUtf8 =
    u8"{\n  \"\uD45C\uC2DC\uBA85\": \"\uC2DC\uD5D8 \uCE90\uB9AD\uD130\", \"creator\": {\"scale\": 1.2500}\n}\n";
const std::string_view sheet{reinterpret_cast<const char *>(sheetUtf8.data()), sheetUtf8.size()};
constexpr std::size_t headerSize = 36;

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(#condition); } while (false)

template<class Error = std::invalid_argument, class Function>
void rejects(Function function)
{
    try { function(); }
    catch (const Error &) { return; }
    throw std::runtime_error("Invalid .nscharacter data was accepted");
}

std::vector<std::uint8_t> read(const std::filesystem::path &path)
{
    std::ifstream stream(path, std::ios::binary);
    CHECK(stream.good());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

void write(const std::filesystem::path &path, std::span<const std::uint8_t> bytes)
{
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    CHECK(stream.good());
}

void checkRoundTrip(std::span<const std::uint8_t> actor, std::span<const std::uint8_t> file)
{
    const auto sections = unpackNSCharacterFile(file);
    CHECK(std::ranges::equal(sections.actorProject, actor));
    CHECK(sections.sheetJson == sheet);
    CHECK(sections.actorProject.data() == file.data() + headerSize);
    CHECK(reinterpret_cast<const std::uint8_t *>(sections.sheetJson.data()) ==
          file.data() + headerSize + actor.size());

    Object3DEngine engine;
    const auto project = importObject3DProject(engine.reference(), sections.actorProject);
    CHECK(project.editor.valid() && project.editor.get().object().valid());
    CHECK(!project.editor.get().object().get().data().meshes.empty());
    const auto currentActor = exportObject3DProject(project);
    const auto currentFile = packNSCharacterFile(currentActor, sections.sheetJson);
    const auto current = unpackNSCharacterFile(currentFile);
    CHECK(current.sheetJson == sheet && std::ranges::equal(current.actorProject, currentActor));
    Object3DEngine restored;
    const auto reopened = importObject3DProject(restored.reference(), current.actorProject);
    CHECK(reopened.editor.get().object().get().data().meshes.size() ==
          project.editor.get().object().get().data().meshes.size());
}

void checkInvalid(std::span<const std::uint8_t> actor)
{
    const auto file = packNSCharacterFile(actor, sheet);
    CHECK(file.size() == headerSize + actor.size() + sheet.size());
    constexpr std::array<std::uint8_t, 16> prefix{
        'N','S','C','H','A','R','\r','\n', 1,0,0,0, 1,0,0,0};
    CHECK(std::ranges::equal(std::span(file).first(prefix.size()), prefix));
    CHECK(file == packNSCharacterFile(actor, sheet));

    for (std::size_t n = 0; n < headerSize; ++n)
        rejects([&] { (void) unpackNSCharacterFile(std::span(file).first(n)); });
    rejects([&] { (void) unpackNSCharacterFile(std::span(file).first(file.size() - 1)); });
    rejects([&] { (void) unpackNSCharacterFile(actor); });
    auto mutated = file;
    mutated.push_back(0);
    rejects([&] { (void) unpackNSCharacterFile(mutated); });
    for (auto offset : {0, 8, 12, 24, 32, 36}) {
        mutated = file;
        mutated[static_cast<std::size_t>(offset)] ^= 0xff;
        rejects([&] { (void) unpackNSCharacterFile(mutated); });
    }
    mutated = file;
    std::fill_n(mutated.begin() + 16, 8, 0xff);
    rejects([&] { (void) unpackNSCharacterFile(mutated); });

    NSCharacterFileLimits limits{actor.size(), sheet.size()};
    CHECK(packNSCharacterFile(actor, sheet, limits) == file);
    CHECK(unpackNSCharacterFile(file, limits).sheetJson == sheet);
    --limits.maximumActorBytes;
    rejects<std::length_error>([&] { (void) packNSCharacterFile(actor, sheet, limits); });
    rejects<std::length_error>([&] { (void) unpackNSCharacterFile(file, limits); });
    ++limits.maximumActorBytes;
    --limits.maximumSheetBytes;
    rejects<std::length_error>([&] { (void) packNSCharacterFile(actor, sheet, limits); });
    rejects<std::length_error>([&] { (void) unpackNSCharacterFile(file, limits); });

    for (const auto invalid : {"", "[]", "null", "{}{}", R"({"x":0,"\u0078":1})",
                               R"({"x":"\ud800"})", R"({"x":NaN})", R"({"x":1e999})",
                               R"({"x":18446744073709551616})", "{\"x\":\"\xff\"}"})
        rejects([&] { (void) packNSCharacterFile(actor, invalid); });

    // A conventional Boost signature cannot pass as either the file or its actor section.
    constexpr std::string_view boostArchiveHeader = "22 serialization::archive 21";
    const auto boostBytes = std::span(reinterpret_cast<const std::uint8_t *>(boostArchiveHeader.data()),
                                      boostArchiveHeader.size());
    rejects([&] { (void) unpackNSCharacterFile(boostBytes); });
    rejects([&] { (void) packNSCharacterFile(boostBytes, sheet); });

    // The document and sheet survive a future actor version; the engine still checks its own schema.
    mutated = file;
    mutated[headerSize + 8] = 0xff;
    const auto future = unpackNSCharacterFile(mutated);
    CHECK(future.sheetJson == sheet);
    Object3DEngine engine;
    rejects([&] { (void) importObject3DProject(engine.reference(), future.actorProject); });
}
} // namespace

int main(int argc, char **argv)
{
    try {
        CHECK(argc >= 3 && argc <= 4);
        const auto actor = read(argv[2]);
        const std::string_view mode = argv[1];
        if (argc == 3 && mode == "roundtrip")
            checkRoundTrip(actor, packNSCharacterFile(actor, sheet));
        else if (argc == 3 && mode == "invalid")
            checkInvalid(actor);
        else if (argc == 4 && mode == "write")
            write(argv[3], packNSCharacterFile(actor, sheet));
        else if (argc == 4 && mode == "read")
            checkRoundTrip(actor, read(argv[3]));
        else if (argc == 4 && mode == "inspect")
            (void) unpackNSCharacterFile(read(argv[3]));
        else
            throw std::runtime_error("Unexpected .nscharacter test arguments");
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
