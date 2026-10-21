#include <array>
#include <cmath>
#include <iiActorObject/Character.h>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>

using namespace iiActorObject;
#define CHECK(c)                                                                                             \
    do                                                                                                       \
    {                                                                                                        \
        if (!(c))                                                                                            \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #c);      \
    } while (false)
template <class E = std::invalid_argument, class F> void rejects(F &&f)
{
    bool caught = false;
    try
    {
        f();
    }
    catch (const E &)
    {
        caught = true;
    }
    CHECK(caught);
}
void near(Vector3 a, Vector3 b, double epsilon = 1e-8)
{
    CHECK(std::abs(a.x - b.x) < epsilon && std::abs(a.y - b.y) < epsilon && std::abs(a.z - b.z) < epsilon);
}
std::shared_ptr<const Skeleton> skeleton()
{
    return std::make_shared<Skeleton>(std::vector<Bone>{{"root", NoBone, {}},
                                                        {"elbow", 0, {{1, 0, 0}}},
                                                        {"hand", 1, {{1, 0, 0}}},
                                                        {"other", 0, {{0, 1, 0}}}});
}
std::shared_ptr<const AnimationClip> clip(std::shared_ptr<const Skeleton> s)
{
    return std::make_shared<AnimationClip>(
        s, 2,
        std::vector<BoneTrack>{
            {0, {{0, {}}, {2, {{2, 0, 0}, Quaternion::fromAxisAngle({0, 0, 1}, std::numbers::pi)}}}}},
        std::vector<AnimationCurve>{{"jawOpen", {{0, 0}, {2, 1}}}});
}
std::shared_ptr<const SkeletalMesh> mesh(std::shared_ptr<const Skeleton> s)
{
    SkeletalMeshLod high;
    high.vertices = {
        {{0, 0, 0}, {0, 0, 1}, {{0, 1}}}, {{1, 0, 0}, {0, 0, 1}, {{1, 2}}}, {{2, 0, 0}, {0, 0, 1}, {{2, 1}}}};
    high.triangles = {0, 1, 2};
    high.morphTargets = {{"smile", {{2, {0, 0, 1}, {}}}}};
    SkeletalMeshLod low;
    low.vertices = {{{2, 0, 0}, {0, 0, 1}, {{0, 1}}}};
    return std::make_shared<SkeletalMesh>(s, std::vector<SkeletalMeshLod>{high, low});
}
void bones()
{
    auto s = skeleton();
    Pose p(s);
    CHECK(s->findBone("hand") == 2 && s->findBone("missing") == NoBone);
    CHECK(s->bones()[1].parent == 0);
    CHECK(p.evaluate() == 4 && p.evaluate() == 0);
    near(p.worldMatrices()[2].transformPoint({}), {2, 0, 0});
    for (auto &m : p.skinMatrices())
        near(m.transformPoint({1, 2, 3}), {1, 2, 3});
    rejects([] { Skeleton s({}); });
    rejects([] { Skeleton s({{"root", 0, {}}}); });
    rejects([] { Skeleton s({{"root", NoBone, {}}, {"root", 0, {}}}); });
    rejects([] { Skeleton s({{"root", NoBone, {{}, {}, {0, 1, 1}}}}); });
    rejects([] { Pose p(nullptr); });
}
void dirty()
{
    auto s = skeleton();
    Pose p(s);
    (void)p.evaluate();
    auto revision = p.revision();
    CHECK(!p.setLocal(2, p.locals()[2]) && revision == p.revision());
    CHECK(p.setLocal(1, {{2, 0, 0}}));
    CHECK(p.evaluate() == 2 && p.evaluate() == 0);
    near(p.worldMatrices()[2].transformPoint({}), {3, 0, 0});
    p.setLocal(3, {{0, 4, 0}});
    CHECK(p.evaluate() == 1);
    Pose copy(s);
    copy.copyFrom(p);
    near(copy.worldMatrices()[2].transformPoint({}), {3, 0, 0});
    p.reset();
    near(p.worldMatrices()[2].transformPoint({}), {2, 0, 0});
    Pose foreign(skeleton());
    rejects([&] { p.copyFrom(foreign); });
    rejects<std::out_of_range>([&] { p.setLocal(100, {}); });
    rejects([&] { p.setLocal(1, {{}, {}, {0, 1, 1}}); });
    CHECK(p.locals()[1] == s->bones()[1].bindTransform);
}
void inverse()
{
    const auto matrix =
        Transform{{3, -4, 5}, Quaternion::fromAxisAngle({1, 2, 3}, .8), {-2, 3, .25}}.matrix();
    const auto inverse = matrix.invertedAffine();
    for (Vector3 point : std::array{Vector3{}, Vector3{1, 2, 3}, Vector3{-20, 4, 5}})
        near(inverse.transformPoint(matrix.transformPoint(point)), point);
    near((inverse * matrix).transformPoint({7, 8, 9}), {7, 8, 9});
    CHECK(matrix.transposed().transposed().elements() == matrix.elements());
    rejects([] { (void)Transform{{}, {}, {0, 1, 1}}.matrix().invertedAffine(); });
}
void animation()
{
    auto s = skeleton();
    auto c = clip(s);
    Pose p(s);
    c->sample(1, p);
    near(p.locals()[0].position, {1, 0, 0});
    near(p.worldMatrices()[2].transformPoint({}), {1, 2, 0});
    CHECK(std::abs(c->sampleCurve(0, 1) - 0.5) < 1e-9);
    c->sample(20, p);
    near(p.locals()[0].position, {2, 0, 0});
    c->sample(2, p, AnimationWrap::Loop);
    near(p.locals()[0].position, {});
    c->sample(-0.5, p, AnimationWrap::Loop);
    near(p.locals()[0].position, {1.5, 0, 0});
    const auto revision = p.revision();
    c->sample(1.5, p);
    CHECK(p.revision() == revision);
    Pose other(skeleton());
    rejects([&] { c->sample(0, other); });
    rejects([&] { c->sample(std::numeric_limits<double>::infinity(), p); });
}
void clipValidation()
{
    auto s = skeleton();
    rejects([&] { AnimationClip c(s, 0, {}); });
    rejects([&] { AnimationClip c(s, 1, {{9, {{0, {}}}}}); });
    rejects([&] { AnimationClip c(s, 1, {{0, {{0, {}}, {0, {}}}}}); });
    rejects([&] { AnimationClip c(s, 1, {{0, {{2, {}}}}}); });
    rejects([&] { AnimationClip c(s, 1, {{0, {{0, {}}}}, {0, {{0, {}}}}}); });
    rejects([&] { AnimationClip c(s, 1, {}, {{"x", {{0, 0}}}, {"x", {{0, 1}}}}); });
    rejects([&] { AnimationClip c(s, 1, {}, {{"x", {}}}); });
}
void player()
{
    auto s = skeleton();
    auto p = std::make_shared<Pose>(s);
    AnimationPlayer a(p);
    CHECK(!a.advance(1));
    a.play(clip(s));
    CHECK(a.advance(.5));
    near(p->locals()[0].position, {.5, 0, 0});
    a.pause();
    CHECK(!a.advance(1) && a.time() == .5);
    a.resume();
    a.setRate(-1);
    a.advance(1);
    CHECK(std::abs(a.time() - 1.5) < 1e-9);
    a.seek(2);
    CHECK(a.time() == 0);
    a.play(clip(s), AnimationWrap::Clamp);
    a.setRate(1);
    a.advance(4);
    CHECK(!a.isPlaying() && a.time() == 2);
    a.stop();
    CHECK(!a.clip() && a.time() == 0);
    near(p->locals()[0].position, {});
    rejects([&] { a.setRate(std::numeric_limits<double>::quiet_NaN()); });
    rejects([&] { a.advance(-1); });
    rejects([&] { a.play(clip(skeleton())); });
    rejects([] { AnimationPlayer bad(nullptr); });
}
void skin()
{
    auto s = skeleton();
    auto m = mesh(s);
    Pose p(s);
    std::array<DeformedVertex, 3> result;
    std::array<double, 1> morph{0};
    m->deform(p, 0, morph, result);
    near(result[2].position, {2, 0, 0});
    CHECK(m->lods()[0].vertices[1].weights[0].weight == 1);
    p.setLocal(0, {{2, 0, 0}});
    m->deform(p, 0, morph, result);
    near(result[2].position, {4, 0, 0});
    near(result[2].normal, {0, 0, 1});
    morph[0] = .5;
    m->deform(p, 0, morph, result);
    near(result[2].position, {4, 0, .5});
    std::array<DeformedVertex, 1> low;
    m->deform(p, 1, {}, low);
    near(low[0].position, {4, 0, 0});
    rejects([&] { m->deform(p, 0, {}, result); });
    Pose wrong(skeleton());
    rejects([&] { m->deform(wrong, 0, morph, result); });
    morph[0] = std::numeric_limits<double>::quiet_NaN();
    rejects([&] { m->deform(p, 0, morph, result); });
}
void normals()
{
    auto s = skeleton();
    SkeletalMeshLod lod;
    lod.vertices = {{{1, 0, 0}, {1, 1, 0}, {{0, 1}}}};
    SkeletalMesh m(s, {lod});
    Pose p(s);
    p.setLocal(0, {{}, {}, {2, 1, 1}});
    std::array<DeformedVertex, 1> out;
    m.deform(p, 0, {}, out);
    near(out[0].position, {2, 0, 0});
    const auto length = std::sqrt(1.25);
    near(out[0].normal, {.5 / length, 1 / length, 0});
}
void meshValidation()
{
    auto s = skeleton();
    SkeletalMeshLod lod;
    lod.vertices = {{{}, {0, 0, 1}, {{0, 1}}}};
    rejects([&] { SkeletalMesh m(s, {}); });
    auto bad = lod;
    bad.vertices[0].weights.clear();
    rejects([&] { SkeletalMesh m(s, {bad}); });
    bad = lod;
    bad.vertices[0].weights[0] = {10, 1};
    rejects([&] { SkeletalMesh m(s, {bad}); });
    bad = lod;
    bad.vertices[0].weights[0].weight = -1;
    rejects([&] { SkeletalMesh m(s, {bad}); });
    bad = lod;
    bad.triangles = {0, 0, 2};
    rejects([&] { SkeletalMesh m(s, {bad}); });
    bad = lod;
    bad.morphTargets = {{"x", {{4, {1, 0, 0}, {}}}}};
    rejects([&] { SkeletalMesh m(s, {bad}); });
    bad = lod;
    bad.morphTargets = {{"x", {{0, {1, 0, 0}, {}}, {0, {2, 0, 0}, {}}}}};
    rejects([&] { SkeletalMesh m(s, {bad}); });
}
void component()
{
    auto s = skeleton();
    auto p = std::make_shared<Pose>(s);
    SkeletalMeshComponent c(mesh(s), p);
    near(c.deformedVertices()[2].position, {2, 0, 0});
    CHECK(c.deformationCount() == 1);
    (void)c.deformedVertices();
    CHECK(c.deformationCount() == 1);
    c.setMorphWeight(0, 0);
    (void)c.deformedVertices();
    CHECK(c.deformationCount() == 1);
    c.setMorphWeight(0, 1);
    near(c.deformedVertices()[2].position, {2, 0, 1});
    CHECK(c.deformationCount() == 2);
    c.setLod(1);
    CHECK(c.deformedVertices().size() == 1 && c.morphWeights().empty());
    rejects<std::out_of_range>([&] { c.setLod(3); });
    rejects([&] { SkeletalMeshComponent bad(mesh(s), std::make_shared<Pose>(skeleton())); });
}
void fk()
{
    Pose p(skeleton());
    std::array controls{JointControl{1, {{3, 0, 0}}, .5}};
    CharacterRig::apply(p, controls);
    near(p.worldMatrices()[2].transformPoint({}), {3, 0, 0});
    const auto revision = p.revision();
    std::array bad{JointControl{1, {{4, 0, 0}}, 1}, JointControl{99, {}, 1}};
    rejects([&] { CharacterRig::apply(p, bad); });
    CHECK(p.revision() == revision);
    controls[0].weight = 2;
    rejects([&] { CharacterRig::apply(p, controls); });
}
void ik()
{
    Pose p(skeleton());
    auto result = CharacterRig::solveTwoBone(p, {0, 1, 2, {1, 1, 0}, {0, 0, 2}, 1});
    CHECK(!result.clamped);
    near(result.reached, {1, 1, 0});
    near(p.worldMatrices()[2].transformPoint({}), {1, 1, 0});
    auto a = p.worldMatrices()[0].transformPoint({}), b = p.worldMatrices()[1].transformPoint({});
    CHECK(std::abs(std::hypot(b.x - a.x, b.y - a.y, b.z - a.z) - 1) < 1e-9);
    p.reset();
    result = CharacterRig::solveTwoBone(p, {0, 1, 2, {4, 0, 0}, {0, 1, 0}, 1});
    CHECK(result.clamped);
    near(result.reached, {2, 0, 0}, 1e-6);
    const auto revision = p.revision();
    rejects([&] { (void)CharacterRig::solveTwoBone(p, {0, 2, 1, {1, 0, 0}, {0, 1, 0}, 1}); });
    CHECK(revision == p.revision());
    p.setLocal(0, {{}, {}, {2, 1, 1}});
    rejects([&] { (void)CharacterRig::solveTwoBone(p, {0, 1, 2, {1, 1, 0}, {0, 1, 0}, 1}); });
}
void ikSweep()
{
    Pose p(skeleton());
    for (int i = 0; i < 160; ++i)
    {
        p.reset();
        const double angle = i * .31;
        const Vector3 target{1.3 * std::cos(angle), 1.3 * std::sin(angle), .25 * std::sin(i * .7)};
        const auto result = CharacterRig::solveTwoBone(p, {0, 1, 2, target, {0, 0, 3}, 1});
        CHECK(!result.clamped);
        near(result.reached, target, 1e-7);
    }
    p.reset();
    (void)CharacterRig::solveTwoBone(p, {0, 1, 2, {0, 0, 0}, {0, 1, 0}, 1});
    near(p.worldMatrices()[2].transformPoint({}), {}, 1e-7);
    auto s = std::make_shared<Skeleton>(
        std::vector<Bone>{{"root", NoBone, {{2, 3, 0}, Quaternion::fromAxisAngle({0, 0, 1}, .6)}},
                          {"shoulder", 0, {{0, 1, 0}}},
                          {"elbow", 1, {{1, 0, 0}}},
                          {"hand", 2, {{1, 0, 0}}}});
    Pose child(s);
    const auto base = child.worldMatrices()[1].transformPoint({});
    const Vector3 target{base.x + .4, base.y + .8, base.z + .3};
    near(CharacterRig::solveTwoBone(child, {1, 2, 3, target, {base.x, base.y, 2}, 1}).reached, target);
}
void face()
{
    auto s = skeleton();
    Pose p(s);
    FacialRig rig(s, {"jawOpen", "blink"}, {{2, 0, {0, -.5, 0}, {0, 0, 1}, .25, {}}},
                  {{"smile", {{0, NoFacialControl, .5, 0}, {1, NoFacialControl, .5, 0}}}},
                  {{"wrinkle", {{0, 1, 2, 0}}}});
    std::array values{1.0, .5};
    std::array<double, 1> shapes{}, maps{};
    rig.evaluate(p, values, shapes, maps);
    near(p.locals()[2].position, {1, -.5, 0});
    CHECK(shapes[0] == .75 && maps[0] == 1);
    const auto rev = p.revision();
    rig.evaluate(p, values, shapes, maps);
    CHECK(p.revision() == rev);
    values[0] = 2;
    rejects([&] { rig.evaluate(p, values, shapes, maps); });
    CHECK(p.revision() == rev);
    rejects([&] { FacialRig bad(s, {"x", "x"}, {}, {}); });
    rejects([&] { FacialRig bad(s, {"x"}, {{9, 0, {}, {1, 0, 0}, 0, {}}}, {}); });
    rejects([&] { FacialRig bad(s, {"x"}, {}, {{"y", {{2, NoFacialControl, 1, 0}}}}); });
}
void character()
{
    auto s = skeleton();
    iiCharacter c(s, "Player");
    auto &part = c.addMesh(mesh(s), "Body");
    c.setPosition({10, 0, 0});
    c.animation().play(clip(s));
    CHECK(c.beginPlay() && c.tick(1));
    near(c.pose().locals()[0].position, {1, 0, 0});
    near(part.worldPosition(), {10, 0, 0});
    CHECK(!c.animationError());
    c.setLod(100);
    CHECK(part.lod() == 1 && part.deformedVertices().size() == 1);
    CHECK(c.removeComponent(part));
    c.setLod(0);
    const auto count = c.componentCount();
    rejects([&] { c.addMesh(mesh(skeleton())); });
    CHECK(c.componentCount() == count);
    CHECK(c.destroy() && !c.animation().isPlaying() && !c.tick(1));
    iiCharacter notStarted(s);
    notStarted.animation().play(clip(s));
    CHECK(notStarted.destroy() && !notStarted.animation().isPlaying());
}
void faceAlias()
{
    auto s = skeleton();
    Pose p(s);
    FacialRig rig(s, {"input"}, {}, {{"shape", {{0, NoFacialControl, 2, 0}}}},
                  {{"map", {{0, NoFacialControl, 1, 0}}}});
    std::array<double, 1> input{.5}, shape{}, map{};
    const auto revision = p.revision();
    rejects([&] { rig.evaluate(p, input, input, map); });
    rejects([&] { rig.evaluate(p, input, shape, input); });
    rejects([&] { rig.evaluate(p, input, shape, shape); });
    CHECK(input[0] == .5 && p.revision() == revision);
    rig.evaluate(p, input, shape, map);
    CHECK(shape[0] == 1 && map[0] == .5);
}
void modular()
{
    auto s = skeleton();
    iiCharacter character(s);
    auto &body = character.addMesh(mesh(s), "Body");
    SkeletalMeshLod faceLod;
    faceLod.vertices = {{{0, 1, 0}, {0, 0, 1}, {{3, 1}}}};
    auto faceMesh = std::make_shared<SkeletalMesh>(s, std::vector<SkeletalMeshLod>{faceLod});
    auto &facePart = character.addMesh(faceMesh, "Face");
    (void)body.deformedVertices();
    (void)facePart.deformedVertices();
    CHECK(body.deformationCount() == 1 && facePart.deformationCount() == 1);
    character.pose().setLocal(3, {{0, 2, 0}});
    near(facePart.deformedVertices()[0].position, {0, 2, 0});
    (void)body.deformedVertices();
    CHECK(body.deformationCount() == 1 && facePart.deformationCount() == 2);
    character.pose().setLocal(0, {{1, 0, 0}});
    (void)body.deformedVertices();
    (void)facePart.deformedVertices();
    CHECK(body.deformationCount() == 2 && facePart.deformationCount() == 3);
}
void clockError()
{
    auto s = skeleton();
    iiCharacter c(s);
    c.animation().play(clip(s));
    c.animation().setRate(std::numeric_limits<double>::max());
    c.beginPlay();
    CHECK(c.tick(std::numeric_limits<double>::max()));
    // Some platforms have wider long double clocks and can represent this step.
    if (c.animationError())
    {
        CHECK(!c.animation().isPlaying());
        c.tick(0);
        CHECK(c.animationError());
        c.animation().setRate(1);
        c.animation().resume();
        c.tick(.25);
        CHECK(!c.animationError());
    }
    c.destroy();
}
void stress()
{
    std::vector<Bone> bones;
    bones.push_back({"root", NoBone, {}});
    for (std::size_t i = 1; i < 1024; ++i)
        bones.push_back({"bone" + std::to_string(i), i - 1, {{1, 0, 0}}});
    auto s = std::make_shared<Skeleton>(std::move(bones));
    Pose p(s);
    CHECK(p.evaluate() == 1024);
    p.setLocal(1023, {{2, 0, 0}});
    CHECK(p.evaluate() == 1);
    near(p.worldMatrices()[1023].transformPoint({}), {1024, 0, 0});
    CHECK(p.evaluate() == 0);
}
int main(int argc, char **argv)
{
    try
    {
        if (argc != 2)
            return 2;
        const std::string name = argv[1];
        if (name == "bones")
            bones();
        else if (name == "dirty")
            dirty();
        else if (name == "inverse")
            inverse();
        else if (name == "animation")
            animation();
        else if (name == "clip_validation")
            clipValidation();
        else if (name == "player")
            player();
        else if (name == "skin")
            skin();
        else if (name == "normals")
            normals();
        else if (name == "mesh_validation")
            meshValidation();
        else if (name == "component")
            component();
        else if (name == "fk")
            fk();
        else if (name == "ik")
            ik();
        else if (name == "ik_sweep")
            ikSweep();
        else if (name == "face")
            face();
        else if (name == "face_alias")
            faceAlias();
        else if (name == "modular")
            modular();
        else if (name == "character")
            character();
        else if (name == "stress")
            stress();
        else if (name == "clock_error")
            clockError();
        else
            return 2;
        std::cout << name << " passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
