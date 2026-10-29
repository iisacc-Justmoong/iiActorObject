#include <filesystem>
#include <fstream>
#include <iiActorObject.h>
#include <iostream>

int main(int argc, char **argv)
{
    try
    {
        using namespace iiActorObject::NSCharacter;
        const std::filesystem::path path = argc > 1 ? argv[1] : "build/character/GeneratedHuman.glb";
        GenerationOptions options;
        if (argc > 2)
            options.resolution = static_cast<unsigned>(std::stoul(argv[2]));
        iiActorObject::Object3DEngine engine;
        const auto asset = HumanoidGenerator::generate(engine, {}, options);
        CharacterEditor editor(asset);
        // A host can drive the same editing object from sliders, sculpt tools, scripts or a UI.
        editor.bakeMorphs(0,
                          std::array{MorphValue{"mouthSmileLeft", .3}, MorphValue{"mouthSmileRight", .3}});
        const auto bytes =
            path.extension() == ".fbx" ? exportFbx(*editor.asset()) : exportGlb(*editor.asset());
        if (!path.parent_path().empty())
            std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char *>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!file)
            throw std::runtime_error("Could not save generated character");
        const auto &data = editor.asset()->data();
        std::cout << "Created " << path << " with " << data.bones.size() << " bones, " << data.meshes.size()
                  << " parts, " << data.meshes[0].geometry.vertices.size() << " body vertices, "
                  << data.meshes[0].geometry.morphTargets.size() << " facial morph channels; "
                  << bytes.size() << " bytes\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
