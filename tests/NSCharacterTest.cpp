#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iiActorObject/Animation.h>
#include <iiActorObject/NSCharacter/NSCharacter.h>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
Object3DEngine &owner()
{
    static Object3DEngine engine;
    return engine;
}
#define CHECK(condition)                                                                                   \
    do                                                                                                     \
    {                                                                                                      \
        if (!(condition))                                                                                  \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) +              \
                                     ": " #condition);                                                     \
    } while (false)
template <class F> void rejects(F &&f)
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
void near(Vector3 a, Vector3 b, double e = 1e-8)
{
    CHECK(std::hypot(a.x - b.x, a.y - b.y, a.z - b.z) < e);
}
CharacterData quad()
{
    CharacterData d;
    d.name = "Editable human";
    d.bones = {{"root", NoBone, {}}, {"pelvis", 0, {{0, 1, 0}}}, {"head", 1, {{0, 1, 0}}}};
    d.semantics = {{"pelvis", 1}, {"head", 2}};
    CharacterMesh m;
    m.name = "Body";
    m.geometry.vertices = {{{-1, -1, 0}, {0, 0, 1}, {{1, 1}}},
                           {{1, -1, 0}, {0, 0, 1}, {{1, 1}}},
                           {{1, 1, 0}, {0, 0, 1}, {{2, 1}}},
                           {{-1, 1, 0}, {0, 0, 1}, {{2, 1}}}};
    m.geometry.triangles = {0, 1, 2, 0, 2, 3};
    m.textureCoordinates = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    d.meshes.push_back(std::move(m));
    return d;
}
auto fixture()
{
    return std::make_shared<CharacterAsset>(owner().createObject(quad()));
}
auto generated()
{
    return HumanoidGenerator::generate(owner(), {}, {36, false, true});
}
void asset()
{
    auto a = fixture();
    CHECK(a->joint("head") == 2 && a->joint("missing") == NoBone);
    auto i = a->instantiate();
    CHECK(i->name() == a->data().name);
    CHECK(i->pose().skeleton() == a->skeleton());
    auto *m = i->findComponent<SkeletalMeshComponent>();
    CHECK(m && m->mesh()->skeleton() == a->skeleton());
    i->pose().setLocal(0, {{2, 0, 0}});
    near(m->deformedVertices()[0].position, {1, -1, 0});
}
void validation()
{
    auto normalized = quad();
    normalized.bones[0].bindTransform.rotation.w = 2;
    CHECK(CharacterAsset(owner().createObject(normalized)).data().bones[0].bindTransform.rotation.w == 1);
    auto d = quad();
    d.meshes[0].textureCoordinates.resize(3);
    rejects([&] { CharacterAsset a(owner().createObject(d)); });
    d = quad();
    d.semantics.push_back({"head", 1});
    rejects([&] { CharacterAsset a(owner().createObject(d)); });
    d = quad();
    d.meshes.push_back(d.meshes[0]);
    rejects([&] { CharacterAsset a(owner().createObject(d)); });
    d = quad();
    d.meshes[0].material = 4;
    rejects([&] { CharacterAsset a(owner().createObject(d)); });
    d = quad();
    d.meshes[0].geometry.triangles[2] = 0;
    rejects([&] { CharacterAsset a(owner().createObject(d)); });
    d = quad();
    d.materials[0].baseColor[0] = std::numeric_limits<double>::quiet_NaN();
    rejects([&] { CharacterAsset a(owner().createObject(d)); });
}
void normals()
{
    auto mesh = quad().meshes[0].geometry;
    for (auto &v : mesh.vertices)
        v.normal = {1, 0, 0};
    recomputeNormals(mesh);
    for (const auto &v : mesh.vertices)
        near(v.normal, {0, 0, 1});
    const auto old = mesh.vertices[0].normal;
    mesh.triangles.push_back(999);
    rejects([&] { recomputeNormals(mesh); });
    near(mesh.vertices[0].normal, old);
}
void binding()
{
    auto d = quad();
    const std::array env{BoneEnvelope{1, {-1, -1, 0}, {-1, 1, 0}, .1},
                         BoneEnvelope{2, {1, -1, 0}, {1, 1, 0}, .1}};
    AutoRigger::bind(d, env, {1, 2});
    CHECK(d.meshes[0].geometry.vertices[0].weights[0].bone == 1);
    CHECK(d.meshes[0].geometry.vertices[1].weights[0].bone == 2);
    CHECK(d.meshes[0].geometry.vertices[1].weights[0].weight == 1);
    rejects([&] { AutoRigger::bind(d, {}, {}); });
    rejects([&] { AutoRigger::bind(d, env, {0, 2}); });
    auto bad = env;
    bad[1].radius = 0;
    rejects([&] { AutoRigger::bind(d, bad); });
    CHECK(d.meshes[0].geometry.vertices[0].weights[0].bone == 1);
    (void)CharacterAsset(owner().createObject(d));
}
void generationFor(unsigned resolution)
{
    auto a = HumanoidGenerator::generate(owner(), {}, {resolution, false, true});
    const auto &g = a->data().meshes[0].geometry;
    CHECK(g.vertices.size() > 500 && g.triangles.size() > 2000);
    CHECK(a->skeleton()->bones().size() >= 60);
    CHECK(a->joint("hand_l") != NoBone && a->joint("jaw") != NoBone);
    CHECK(g.morphTargets.size() == 52);
    Pose p(a->skeleton());
    std::vector<DeformedVertex> out(g.vertices.size());
    std::vector<double> morphs(g.morphTargets.size());
    a->meshes()[0]->deform(p, 0, morphs, out);
    for (std::size_t i = 0; i < out.size(); ++i)
        near(out[i].position, g.vertices[i].position);
    // A closed body, rather than intersecting standalone tubes: every edge occurs twice.
    std::map<std::pair<std::uint32_t, std::uint32_t>, unsigned> edges;
    for (std::size_t i = 0; i < g.triangles.size(); i += 3)
        for (unsigned j = 0; j < 3; ++j)
        {
            auto x = g.triangles[i + j], y = g.triangles[i + (j + 1) % 3];
            if (x > y)
                std::swap(x, y);
            ++edges[{x, y}];
        }
    for (const auto &[edge, count] : edges)
    {
        (void)edge;
        CHECK(count == 2);
    }
    std::vector<std::vector<std::uint32_t>> neighbors(g.vertices.size());
    for (const auto &[edge, count] : edges)
    {
        (void)count;
        neighbors[edge.first].push_back(edge.second);
        neighbors[edge.second].push_back(edge.first);
    }
    std::set<std::uint32_t> visited{0};
    std::vector<std::uint32_t> queue{0};
    for (std::size_t i = 0; i < queue.size(); ++i)
        for (auto v : neighbors[queue[i]])
            if (visited.insert(v).second)
                queue.push_back(v);
    CHECK(visited.size() == g.vertices.size());
    auto bad = BodyParameters{};
    bad.height = -1;
    rejects([&] { (void)HumanoidGenerator::generate(owner(), bad, {36, false, false}); });
    rejects([] { (void)HumanoidGenerator::generate(owner(), {}, {4, false, false}); });
}
void generation()
{
    for (unsigned resolution : {32, 36, 96, 160})
        generationFor(resolution);
}
void topology()
{
    const auto a = generated();
    BodyParameters p;
    p.height = 2.05;
    p.legRatio = .55;
    p.shoulderWidth = .46;
    p.headScale = 1.1;
    p.noseScale = 1.2;
    auto b = HumanoidGenerator::generate(owner(), p, {36, false, true});
    const auto &x = a->data().meshes[0].geometry, &y = b->data().meshes[0].geometry;
    CHECK(x.triangles == y.triangles && x.vertices.size() == y.vertices.size());
    CHECK(x.vertices[0].position != y.vertices[0].position);
    CHECK(a->data().bones[a->joint("head")].bindTransform !=
          b->data().bones[b->joint("head")].bindTransform);
}
void edits()
{
    CharacterEditor e(fixture());
    const auto original = e.asset();
    const std::array edits{VertexMove{0, {0, 0, .25}}, VertexMove{1, {0, 0, .1}}};
    e.moveVertices(0, edits);
    near(e.asset()->data().meshes[0].geometry.vertices[0].position, {-1, -1, .25});
    near(original->data().meshes[0].geometry.vertices[0].position, {-1, -1, 0});
    const auto snapshot = e.asset();
    auto revision = e.revision();
    auto bad = edits;
    bad[1].vertex = 99;
    rejects([&] { e.moveVertices(0, bad); });
    CHECK(e.asset() == snapshot && e.revision() == revision);
    CHECK(e.undo() && e.asset() == original && e.redo() && e.asset() == snapshot);
}
void sculpt()
{
    CharacterEditor e(fixture());
    e.sculpt(0, {-1, -1, 0}, .5, {0, 0, .3}, true);
    const auto &v = e.asset()->data().meshes[0].geometry.vertices;
    near(v[0].position, {-1, -1, .3});
    near(v[1].position, {1, -1, .3});
    near(v[2].position, {1, 1, 0});
    rejects([&] { e.sculpt(0, {}, 0, {0, 0, 1}); });
    CHECK(e.undo());
    e.smooth(0, .1, 2);
    CHECK(e.asset()->data().meshes[0].geometry.vertices[0].position.x > -1);
}
void morphs()
{
    CharacterEditor e(fixture());
    std::vector<Vector3> target;
    for (const auto &v : e.asset()->data().meshes[0].geometry.vertices)
        target.push_back(v.position);
    target[2].z = .4;
    e.createMorph(0, "smile", target);
    CHECK(e.asset()->data().meshes[0].geometry.morphTargets.size() == 1);
    e.bakeMorphs(0, std::array{MorphValue{"smile", .5}});
    near(e.asset()->data().meshes[0].geometry.vertices[2].position, {1, 1, .2});
    auto revision = e.revision();
    rejects([&] { e.bakeMorphs(0, std::array{MorphValue{"missing", 1}}); });
    CHECK(e.revision() == revision);
    CHECK(e.undo());
    near(e.asset()->data().meshes[0].geometry.vertices[2].position, {1, 1, 0});
}
void weights()
{
    CharacterEditor e(fixture());
    e.setWeights(0, std::array{VertexWeights{0, {{1, 2}, {2, 6}}}});
    CHECK(e.asset()->data().meshes[0].geometry.vertices[0].weights[0].weight == .25);
    auto revision = e.revision();
    rejects([&] { e.setWeights(0, std::array{VertexWeights{0, {{99, 1}}}}); });
    CHECK(e.revision() == revision);
    e.smoothWeights(0, .5, 1, 2);
    const auto &w = e.asset()->data().meshes[0].geometry.vertices[1].weights;
    CHECK(w.size() == 2);
    CHECK(std::abs(w[0].weight + w[1].weight - 1) < 1e-12);
}
void joints()
{
    CharacterEditor e(fixture());
    e.moveJoint(1, {.5, 1, 0}, true);
    Pose p(e.asset()->skeleton());
    near(p.worldMatrices()[1].transformPoint({}), {.5, 1, 0});
    near(p.worldMatrices()[2].transformPoint({}), {0, 2, 0});
    // Editing a bind joint recalculates inverse binds: the neutral mesh is unchanged.
    auto c = e.asset()->instantiate();
    near(c->findComponent<SkeletalMeshComponent>()->deformedVertices()[0].position, {-1, -1, 0});
    e.renameJoint(2, "cranium");
    CHECK(e.asset()->joint("head") == 2 && e.asset()->skeleton()->findBone("cranium") == 2);
    auto revision = e.revision();
    rejects([&] { e.renameJoint(2, "root"); });
    CHECK(e.revision() == revision);
}
void history()
{
    CharacterEditor e(fixture(), 2);
    e.renameJoint(2, "head1");
    e.renameJoint(2, "head2");
    e.renameJoint(2, "head3");
    CHECK(e.undo() && e.undo() && !e.undo());
    CHECK(e.redo());
    e.renameJoint(2, "newHead");
    CHECK(!e.redo());
    CHECK(e.editLabel() == "Rename joint");
    rejects([] { CharacterEditor e(nullptr); });
}
void glb(const std::filesystem::path &path, bool fbx = false)
{
    auto definition = quad();
    definition.materials[0].baseColor[3] = .6;
    CharacterEditor e(std::make_shared<CharacterAsset>(owner().createObject(std::move(definition))));
    std::vector<Vector3> target;
    for (const auto &v : e.asset()->data().meshes[0].geometry.vertices)
        target.push_back(v.position);
    target[2].z = .5;
    e.createMorph(0, "smile", target);
    e.setMorph(0, {"translate",
                   {{0, {.25, .5, .75}, {}},
                    {1, {.25, .5, .75}, {}},
                    {2, {.25, .5, .75}, {}},
                    {3, {.25, .5, .75}, {}}}});
    auto bytes = fbx ? exportFbx(*e.asset()) : exportGlb(*e.asset());
    CHECK(bytes.size() > 100);
    if (fbx)
        CHECK(std::string(bytes.begin(), bytes.begin() + 18) == "Kaydara FBX Binary");
    else
        CHECK(bytes[0] == 'g' && bytes[1] == 'l' && bytes[2] == 'T' && bytes[3] == 'F');
    CHECK(bytes == (fbx ? exportFbx(*e.asset()) : exportGlb(*e.asset())));
    if (!path.empty())
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        CHECK(out.good());
    }
}
void fbxIo()
{
    auto d = quad();
    d.materials[0].baseColor[3] = .6;
    d.materials[0].roughness = .37;
    d.materials[0].metallic = .15;
    d.semantics.push_back({"cranium", 2});
    d.meshes[0].geometry.morphTargets = {{"smile", {{2, {0, 0, .5}, {}}}}};
    CharacterAsset source(owner().createObject(d));
    const auto bytes = exportFbx(source);
    const auto parsed = importFbx(owner(), bytes);
    CHECK(parsed.asset && parsed.warnings.empty());
    CHECK(parsed.asset->data().name == source.data().name);
    CHECK(parsed.asset->data().bones.size() == 3 && parsed.asset->data().meshes.size() == 1);
    CHECK(parsed.asset->joint("cranium") == 2);
    CHECK(std::abs(parsed.asset->data().materials[0].baseColor[3] - .6) < 1e-8);
    CHECK(std::abs(parsed.asset->data().materials[0].roughness - .37) < 1e-8);
    CHECK(std::abs(parsed.asset->data().materials[0].metallic - .15) < 1e-8);
    const auto &g = parsed.asset->data().meshes[0].geometry;
    CHECK(g.vertices.size() == 4 && g.triangles.size() == 6 && g.morphTargets.size() == 1);
    CHECK(g.morphTargets[0].name == "smile");
    auto instance = parsed.asset->instantiate();
    auto *part = instance->findComponent<SkeletalMeshComponent>();
    instance->pose().setLocal(2, {{1, 1, 0}});
    part->setMorphWeight(0, .5);
    near(part->deformedVertices()[2].position, {2, 1, .25});
    rejects([&] { (void)importFbx(owner(), bytes, {bytes.size() - 1, 100, 100, 100}); });
    rejects([&] { (void)importFbx(owner(), bytes, {bytes.size(), 3, 100, 100}); });
    rejects([&] { (void)importFbx(owner(), std::array<std::uint8_t, 4>{1, 2, 3, 4}); });
    auto head=source.skeleton()->bones()[2].bindTransform,moved=head;moved.position.x=.4;
    const AnimationClip clip(source.skeleton(),1,{{2,{{0,head},{1,moved}}}},{{"smile",{{0,0},{1,1}}}});
    iiActorObject::FbxAnimationExportOptions animationOptions;animationOptions.exportMatchingMorphCurves=true;
    const auto animated=exportFbx(source,clip,animationOptions);
    std::shared_ptr<const CharacterAsset> borrowed;
    {
        Object3DEngine temporary;
        const auto result=importFbxWithAnimations(temporary,animated);
        CHECK(result.asset && result.animations.size()==1 && result.asset->valid());
        CHECK(result.animations[0].clip->skeleton().get()==result.asset->skeleton().get());
        Pose pose(result.asset->skeleton());result.animations[0].clip->sample(.5,pose);
        CHECK(std::abs(pose.locals()[result.asset->joint("head")].position.x-.2)<1e-5);
        CHECK(result.animations[0].clip->curves().size()==1);
        CHECK(std::abs(result.animations[0].clip->sampleCurve(0,.5)-.5)<1e-5);
        CHECK(&result.asset->data()==&result.asset->object().get().data());borrowed=result.asset;
    }
    CHECK(borrowed&&!borrowed->valid());
}
void fbxTransforms()
{
    auto d = quad();
    d.bones[0].bindTransform = {{2, -3, 1}, Quaternion::fromAxisAngle({1, 2, 3}, .7), {1.2, 1.2, 1.2}};
    d.bones[1].bindTransform.rotation = Quaternion::fromAxisAngle({1, 0, 0}, .35);
    d.bones[1].bindTransform.scale = {.8, 1.1, 1};
    d.bones[2].bindTransform.rotation = Quaternion::fromAxisAngle({0, 0, 1}, -.5);
    CharacterAsset source(owner().createObject(d));
    auto result = importFbx(owner(), exportFbx(source));
    Pose expected(source.skeleton()), actual(result.asset->skeleton());
    for (std::size_t i = 0; i < 3; ++i)
        for (Vector3 p : std::array{Vector3{}, Vector3{1, 2, 3}})
            near(expected.worldMatrices()[i].transformPoint(p), actual.worldMatrices()[i].transformPoint(p),
                 1e-6);
    auto instance = result.asset->instantiate();
    auto *part = instance->findComponent<SkeletalMeshComponent>();
    for (std::size_t i = 0; i < 4; ++i)
        near(part->deformedVertices()[i].position, d.meshes[0].geometry.vertices[i].position, 1e-6);
}
void influences(const std::filesystem::path &path)
{
    auto d = quad();
    d.bones = {{"root", NoBone, {}}};
    for (std::size_t i = 1; i <= 12; ++i)
        d.bones.push_back({"joint_" + std::to_string(i), 0, {{0, i * .1, 0}}});
    for (auto &v : d.meshes[0].geometry.vertices)
    {
        v.weights.clear();
        for (std::size_t i = 1; i <= 12; ++i)
            v.weights.push_back({i, 1});
    }
    const CharacterAsset source(owner().createObject(d));
    const auto imported = importFbx(owner(), exportFbx(source));
    CHECK(imported.asset->data().meshes[0].geometry.vertices[0].weights.size() == 12);
    auto instance = imported.asset->instantiate();
    auto t = instance->pose().locals()[12];
    t.position.x += 1;
    instance->pose().setLocal(12, t);
    near(instance->findComponent<SkeletalMeshComponent>()->deformedVertices()[0].position,
         {-1 + 1.0 / 12, -1, 0});
    const auto bytes = exportGlb(source);
    if (!path.empty())
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        CHECK(out.good());
    }
}
void fbxForeign(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    CHECK(file.good());
    const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file),
                                          std::istreambuf_iterator<char>()};
    const auto imported = importFbx(owner(), bytes);
    CHECK(imported.asset->data().bones.size() == 71 && imported.asset->data().meshes.size() == 3);
    CHECK(imported.asset->joint("hand_l") != NoBone && imported.asset->joint("head") != NoBone);
    const auto found =
        std::find_if(imported.asset->data().meshes.begin(), imported.asset->data().meshes.end(),
                     [](const auto &m) { return m.name == "Body"; });
    CHECK(found != imported.asset->data().meshes.end());
    const auto reference = HumanoidGenerator::generate(owner(), {}, {96, true, true});
    CHECK(found->geometry.vertices.size() >= reference->data().meshes[0].geometry.vertices.size() &&
          found->geometry.morphTargets.size() == 52);
    auto runtime = imported.asset->instantiate();
    auto *body = runtime->findComponent<SkeletalMeshComponent>();
    CHECK(body);
    const auto original = body->deformedVertices()[0].position;
    auto root = runtime->pose().locals()[0];
    root.position.x += 1;
    runtime->pose().setLocal(0, root);
    const auto transformed = body->deformedVertices()[0].position;
    near(transformed, {original.x + 1, original.y, original.z}, 1e-5);
    CharacterEditor editor(imported.asset);
    editor.sculpt(0, found->geometry.vertices[0].position, .01, {0, 0, .0001});
    CHECK(editor.undo());
}
int main(int argc, char **argv)
{
    try
    {
        if (argc < 2)
            return 2;
        const std::string n = argv[1];
        if (n == "asset")
            asset();
        else if (n == "validation")
            validation();
        else if (n == "normals")
            normals();
        else if (n == "binding")
            binding();
        else if (n == "generation")
            generation();
        else if (n == "topology")
            topology();
        else if (n == "edits")
            edits();
        else if (n == "sculpt")
            sculpt();
        else if (n == "morphs")
            morphs();
        else if (n == "weights")
            weights();
        else if (n == "joints")
            joints();
        else if (n == "history")
            history();
        else if (n == "glb")
            glb(argc > 2 ? argv[2] : "");
        else if (n == "fbx")
            glb(argc > 2 ? argv[2] : "", true);
        else if (n == "fbx_io")
            fbxIo();
        else if (n == "fbx_transforms")
            fbxTransforms();
        else if (n == "influences")
            influences(argc > 2 ? argv[2] : "");
        else if (n == "fbx_foreign" && argc > 2)
            fbxForeign(argv[2]);
        else
            return 2;
        std::cout << n << " passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
