#include <iiActorObject/Features.h>
#include <iiActorObject/ImplicitSurface.h>
#include <iiActorObject/MeshPrimitives.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DInterchange.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/NSCharacter.h>
#endif
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <type_traits>

using namespace iiActorObject;
static_assert(!std::is_copy_constructible_v<Object3DAsset>);
static_assert(!std::is_copy_constructible_v<Object3DEngine>);
static_assert(!std::is_copy_constructible_v<Object3DEditor>);
static_assert(!std::is_move_constructible_v<Object3DEditor>);
#define CHECK(x)                                                                                           \
    do                                                                                                     \
    {                                                                                                      \
        if (!(x))                                                                                          \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #x);    \
    } while (false)
template <class F> void rejects(F f)
{
    bool caught = false;
    try
    {
        f();
    }
    catch (const std::exception &)
    {
        caught = true;
    }
    CHECK(caught);
}
Object3DData triangle()
{
    Object3DData d;
    Object3DMesh m;
    m.name = "Triangle";
    m.geometry.vertices = {
        {{0, 0, 0}, {0, 0, 1}, {}}, {{1, 0, 0}, {0, 0, 1}, {}}, {{0, 1, 0}, {0, 0, 1}, {}}};
    m.geometry.triangles = {0, 1, 2};
    d.meshes.push_back(m);
    return d;
}
#if IIACTOROBJECT_HAS_NSCHARACTER
void ownership()
{
    static_assert(std::is_same_v<NSCharacter::CharacterData, Object3DData>);
    std::shared_ptr<const NSCharacter::CharacterAsset> character;
    std::unique_ptr<NSCharacter::CharacterEditor> editor;
    Object3DReference reference;
    Object3DHandle object;
    {
        Object3DEngine engine;
        reference = engine.reference();
        object = engine.createObject(triangle());
        character = std::make_shared<NSCharacter::CharacterAsset>(object);
        editor = std::make_unique<NSCharacter::CharacterEditor>(character);
        CHECK(engine.objectCount() == 1 && engine.editorCount() == 1);
        CHECK(&character->data() == &object.get().data());
        CHECK(character->object() == object && character->valid());
    }
    CHECK(!reference.valid() && !object.valid() && !character->valid());
    rejects([&] { (void)character->data(); });
    rejects([&] { (void)editor->asset(); });
    rejects([&] { (void)reference.createObject(triangle()); });
}
#endif
void generic()
{
    Object3DEngine engine;
    auto object = engine.createObject(triangle());
    CHECK(!object.get().hasRig());
    CHECK(object.get().data().bones.size() == 1 &&
          object.get().data().meshes[0].geometry.vertices[0].weights[0].weight == 1);
    auto session = engine.editObject(object, 2);
    for (unsigned i = 0; i < 12; ++i)
        session.get().moveVertices(0, std::array{VertexMove{0, {0, 0, .001}}});
    engine.collectGarbage();
    CHECK(engine.objectCount() <= 4);
    CHECK(session.get().undo() && session.get().undo() && !session.get().undo());
    auto bytes = exportObject3DFbx(session.get().object().get());
    auto imported = importObject3DFbx(engine.reference(), bytes);
    CHECK(imported.object.valid());
    CHECK(!session.get().object().get().hasRig() && !imported.object.get().hasRig());
    CHECK(exportObject3DGlb(imported.object.get()).size() > 100);
    const auto id = session.get().object().id();
    CHECK(engine.eraseObject(id));
    CHECK(!session.get().object().valid());
}
void collection()
{
    Object3DEngine engine;
    auto reference = engine.reference();
    {
        auto object = engine.createObject(triangle());
        auto editor = engine.editObject(object);
        CHECK(editor.valid());
    }
    CHECK(engine.collectGarbage() == 2);
    CHECK(engine.objectCount() == 0 && engine.editorCount() == 0);
    Object3DEngine other;
    auto object = other.createObject(triangle());
    rejects([&] { (void)engine.editObject(object); });
    auto own = engine.createObject(triangle());
    Object3DEngine moved(std::move(engine));
    CHECK(reference.valid() && own.valid() && moved.reference().owns(own));
}
void surface()
{
    Object3DEngine engine;
    auto geometry =
        ImplicitSurfaceMesher::generate([](Vector3 p) { return std::hypot(p.x, p.y, p.z) - .6; },
                                        {{-1, -1, -1}, {1, 1, 1}, {12, 12, 12}, false});
    CHECK(geometry.vertices.size() > 100);
    const auto primitive = MeshPrimitives::ellipsoid({}, {.2, .3, .4});
    CHECK(primitive.geometry.vertices.size() == 266);
    rejects([] { (void)MeshPrimitives::ellipsoid({}, {0, 1, 1}); });
    Object3DData data;
    Object3DMesh mesh;
    mesh.name = "Sphere";
    mesh.geometry = std::move(geometry);
    data.meshes.push_back(std::move(mesh));
    auto object = engine.createObject(std::move(data));
    CHECK(!object.get().hasRig());
    rejects(
        [&]
        {
            (void)ImplicitSurfaceMesher::generate([](Vector3)
                                                  { return std::numeric_limits<double>::quiet_NaN(); });
        });
    CHECK(std::abs(capsuleSignedDistance({0, 1, 0}, {}, {}, .5) - .5) < 1e-12);
}
void limits()
{
    Object3DEngine engine({2, 1});
    auto a = engine.createObject(triangle()), b = engine.createObject(triangle());
    rejects([&] { (void)engine.createObject(triangle()); });
    CHECK(engine.objectCount() == 2);
    auto editor = engine.editObject(a);
    rejects([&] { (void)engine.editObject(b); });
    const auto rev = editor.get().revision();
    auto invalid = triangle();
    invalid.meshes[0].geometry.triangles[2] = 42;
    rejects([&] { editor.get().replace(invalid); });
    CHECK(editor.get().revision() == rev);
    CHECK(engine.eraseEditor(editor.id()) && !editor.valid());
    auto removed = a;
    CHECK(engine.eraseObject(a.id()) && !removed.valid());
    rejects([&] { (void)removed.get(); });
}
int main(int argc, char **argv)
{
    try
    {
        if (argc != 2)
            return 2;
        const std::string c = argv[1];
        if (c == "generic")
            generic();
#if IIACTOROBJECT_HAS_NSCHARACTER
        else if (c == "ownership")
            ownership();
#endif
        else if (c == "collection")
            collection();
        else if (c == "surface")
            surface();
        else if (c == "limits")
            limits();
        else
            return 2;
        std::cout << c << " passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
