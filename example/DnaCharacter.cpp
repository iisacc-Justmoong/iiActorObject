#include <iiActorObject/NSCharacter/DnaCharacter.h>
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
int main(int argc, char **argv)
{
    try
    {
        if (argc != 3) throw std::invalid_argument("Usage: iiActorObjectDnaCharacter input.dna output.glb|output.fbx|output.dna");
        std::ifstream input(argv[1],std::ios::binary|std::ios::ate);
        if (!input || input.tellg() < 0 || input.tellg() > 256*1024*1024)
            throw std::runtime_error("DNA file is missing or exceeds 256 MiB");
        std::vector<std::byte> source(static_cast<std::size_t>(input.tellg()));
        input.seekg(0);
        input.read(reinterpret_cast<char *>(source.data()),static_cast<std::streamsize>(source.size()));
        if (!input) throw std::runtime_error("DNA read failed");
        iiActorObject::Object3DEngine engine;
        auto character=iiActorObject::NSCharacter::importDna(engine,source);
        auto actor=character.asset->instantiate();
        auto neutral=character.rig.get().evaluateNamed({});
        character.rig.get().applyTo(*actor,neutral);
        const std::filesystem::path output(argv[2]);
        const auto extension=output.extension().string();
        if (extension!=".glb" && extension!=".fbx" && extension!=".dna")
            throw std::invalid_argument("Output extension must be .glb, .fbx or .dna");
        const auto write=[&](std::span<const std::byte> bytes)
        {
            if (!output.parent_path().empty()) std::filesystem::create_directories(output.parent_path());
            std::ofstream file(output,std::ios::binary);
            file.write(reinterpret_cast<const char *>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
            if (!file) throw std::runtime_error("Character output failed");
        };
        if (extension==".dna")
        {
            auto serialized=iiActorObject::NSCharacter::exportDna(character);
            write(serialized.bytes);
            for(const auto &warning:serialized.warnings) std::cout<<warning<<'\n';
        }
        else
        {
            const auto bytes=extension==".fbx" ? iiActorObject::NSCharacter::exportFbx(*character.asset) :
                                               iiActorObject::NSCharacter::exportGlb(*character.asset);
            write(std::as_bytes(std::span(bytes)));
        }
        std::cout << "DNA mesh parts: " << character.asset->meshes().size()
                  << ", joints: " << character.asset->data().bones.size()
                  << ", raw controls: " << character.rig.get().definition().rawControls.size() << '\n';
        for (const auto &warning:character.warnings) std::cout << warning << '\n';
    }
    catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
