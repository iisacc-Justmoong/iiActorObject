#include <cmath>
#include <filesystem>
#include <fstream>
#include <iiActorObject/ImplicitSurface.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iostream>

int main(int argc, char **argv)
{
    try
    {
        using namespace iiActorObject;
        Object3DEngine engine;
        Object3DData definition;
        Object3DMesh mesh;
        mesh.name = "Surface";
        mesh.geometry =
            ImplicitSurfaceMesher::generate([](Vector3 p) { return std::hypot(p.x, p.y, p.z) - .7; });
        definition.meshes.push_back(std::move(mesh));
        const auto object = engine.createObject(std::move(definition));
        const auto editor = engine.editObject(object);
        editor.get().sculpt(0, {0, .7, 0}, .25, {0, .05, 0});
        const std::filesystem::path path = argc > 1 ? argv[1] : "build/object3d/Sphere.glb";
        const auto bytes = path.extension() == ".fbx" ? exportObject3DFbx(editor.get().object().get())
                                                      : exportObject3DGlb(editor.get().object().get());
        if (!path.parent_path().empty())
            std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char *>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!file)
            throw std::runtime_error("Cannot save 3D object");
        std::cout << "Core 3D object saved: " << path << '\n';
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
