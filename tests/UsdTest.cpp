#include <iiActorObject.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <numbers>
#include <stdexcept>

using namespace iiActorObject;
#define CHECK(condition) do { if (!(condition)) throw std::runtime_error( \
    std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #condition); } while (false)

template<class F> void invalid(F&& function)
{
    bool caught = false;
    try { function(); } catch (const std::invalid_argument&) { caught = true; }
    CHECK(caught);
}

UsdLayer one(const Actor& actor, std::string name = {}, std::vector<UsdAssetReference> references = {})
{
    const std::array placements{UsdActorPlacement{&actor, std::move(name), std::move(references)}};
    return exportUsdLayer(placements);
}

void matrix()
{
    const Transform transform{{10, 20, 30}, Quaternion::fromAxisAngle({0, 0, 1}, 0.7), {2, -3, 0.5}};
    const auto native = transform.matrix();
    const auto usd = toUsdMatrix(native);
    CHECK(usd[3][0] == 10 && usd[3][1] == 20 && usd[3][2] == 30);
    CHECK(usd[0][3] == 0 && usd[1][3] == 0 && usd[2][3] == 0 && usd[3][3] == 1);
    const Vector3 p{3, 4, 5};
    const auto expected = native.transformPoint(p);
    const Vector3 actual{p.x * usd[0][0] + p.y * usd[1][0] + p.z * usd[2][0] + usd[3][0],
        p.x * usd[0][1] + p.y * usd[1][1] + p.z * usd[2][1] + usd[3][1],
        p.x * usd[0][2] + p.y * usd[1][2] + p.z * usd[2][2] + usd[3][2]};
    CHECK(std::abs(actual.x - expected.x) < 1e-12);
    CHECK(std::abs(actual.y - expected.y) < 1e-12);
    CHECK(std::abs(actual.z - expected.z) < 1e-12);
    const auto huge = Transform{{}, {}, {1e308, 1e308, 1e308}}.matrix();
    invalid([&] { (void)toUsdMatrix(huge * huge); });
}

void layer()
{
    Actor actor("Quoted \"name\"\\path\n\t\xED\x95\x9C\xEA\xB8\x80");
    actor.addTag("Player");
    actor.addTag("quote\" and newline\n");
    actor.setPosition({10, 20, 30});
    const auto result = one(actor, "Player");
    CHECK(result.usda.starts_with("#usda 1.0\n"));
    CHECK(result.usda.find("defaultPrim = \"World\"") != std::string::npos);
    CHECK(result.usda.find("upAxis = \"Z\"") != std::string::npos);
    CHECK(result.usda.find("metersPerUnit = 1") != std::string::npos);
    CHECK(result.usda.find("Quoted \\\"name\\\"\\\\path\\n\\t") != std::string::npos);
    CHECK(result.usda.find("custom uint64 iiActor:id") != std::string::npos);
    CHECK(result.usda.find("custom string[] iiActor:tags") != std::string::npos);
    CHECK(result.actors.size() == 1 && result.actors[0].actorId == actor.id());
    CHECK(result.actors[0].primPath == "/World/Player");
    CHECK(result.actors[0].components.size() == 1);
    CHECK(result.actors[0].components[0].componentIndex == 0);
    CHECK(result.actors[0].components[0].primPath == "/World/Player");
    CHECK(one(actor, "Player").usda == result.usda);
    CHECK(actor.state() == LifecycleState::Created);
    CHECK(one(actor).actors[0].primPath == "/World/Actor_" + std::to_string(actor.id().value));
    CHECK(exportUsdLayer({}).actors.empty());
    struct Comma final : std::numpunct<char> { char do_decimal_point() const override { return ','; } };
    const auto old = std::locale();
    std::locale::global(std::locale(old, new Comma));
    const auto underLocale = one(actor, "Player");
    std::locale::global(old);
    CHECK(underLocale.usda == result.usda);
}

void hierarchy()
{
    Actor actor;
    auto& child = actor.createComponent<SceneComponent>("duplicate");
    auto& parent = actor.createComponent<SceneComponent>("duplicate");
    auto& detached = actor.createComponent<SceneComponent>("detached");
    actor.createComponent<ActorComponent>("behavior");
    CHECK(child.attachTo(&parent) && detached.attachTo(nullptr));
    const auto result = one(actor, "Entity");
    const auto& bindings = result.actors[0].components;
    CHECK(bindings.size() == 5);
    CHECK(bindings[1].primPath == "/World/Entity/Component_2/Component_1");
    CHECK(bindings[2].primPath == "/World/Entity/Component_2");
    CHECK(bindings[3].primPath == "/World/Entity/Component_3");
    CHECK(result.usda.find("!resetXformStack!") != std::string::npos);
    CHECK(result.usda.find("def Scope \"Component_4\"") != std::string::npos);
    CHECK(result.usda.find("custom token iiActor:componentType = \"behavior\"") != std::string::npos);
}

void references()
{
    Actor actor;
    auto& scene = actor.createComponent<SceneComponent>();
    const auto result = one(actor, "Entity", {{nullptr, "assets/model@v1.usda", "", true},
        {&scene, "assets\\shape.usda", "/Shape", false}});
    CHECK(result.usda.find("@@@assets/model@v1.usda@@@") != std::string::npos);
    CHECK(result.usda.find("@assets/shape.usda@</Shape>") != std::string::npos);
    CHECK(result.usda.find("instanceable = true") != std::string::npos);
    CHECK(result.usda.find("def \"Asset_0\"") != std::string::npos);
    CHECK(result.actors[0].assets[0].primPath == "/World/Entity/Asset_0");
    CHECK(result.actors[0].assets[1].primPath == "/World/Entity/Component_1/Asset_1");
    CHECK(result.actors[0].assets[1].referenceIndex == 1);
}

void validation()
{
    Actor actor, other;
    const std::array valid{UsdActorPlacement{&actor, "Entity"}};
    for (const auto& name : {"bad/path", "1bad", "bad:name", ".", "with space"}) {
        invalid([&] { (void)one(actor, name); });
        UsdStageOptions options;
        options.rootPrimName = name;
        invalid([&] { (void)exportUsdLayer(valid, options); });
    }
    UsdStageOptions options;
    options.rootPrimName.clear();
    invalid([&] { (void)exportUsdLayer(valid, options); });
    options = {};
    for (double unit : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        options.metersPerUnit = unit;
        invalid([&] { (void)exportUsdLayer(valid, options); });
    }
    options = {};
    options.upAxis = static_cast<UsdUpAxis>(99);
    invalid([&] { (void)exportUsdLayer(valid, options); });
    const std::array nullPlacement{UsdActorPlacement{}};
    invalid([&] { (void)exportUsdLayer(nullPlacement); });
    const std::array duplicates{UsdActorPlacement{&actor, "Same"}, UsdActorPlacement{&other, "Same"}};
    invalid([&] { (void)exportUsdLayer(duplicates); });
    const std::array repeated{UsdActorPlacement{&actor, "A"}, UsdActorPlacement{&actor, "B"}};
    invalid([&] { (void)exportUsdLayer(repeated); });
    invalid([&] { (void)one(actor, {}, {{&other.rootComponent(), "a.usda"}}); });
    for (const auto& asset : {"", "bad@@@name.usda", "new\nline.usda"})
        invalid([&] { (void)one(actor, {}, {{nullptr, asset}}); });
    for (const auto& prim : {"relative", "/A.attr", "/", "/A/", "/A//B", "/A{v=x}"})
        invalid([&] { (void)one(actor, {}, {{nullptr, "a.usda", prim}}); });
    actor.setName(std::string(1, static_cast<char>(0xff)));
    invalid([&] { (void)one(actor); });
    actor.setName(std::string("x\0y", 3));
    invalid([&] { (void)one(actor); });
    actor.setName("Valid");
    CHECK(actor.destroy());
    invalid([&] { (void)one(actor); });
}

void fixture(const std::filesystem::path& directory)
{
    std::filesystem::create_directories(directory);
    const std::string name = "Player \"quoted\"\\path\n\t\xED\x95\x9C\xEA\xB8\x80";
    Actor actor(name), second("Second");
    actor.addTag("Interactive");
    actor.addTag("\" ) { def Xform \"Injected\" {} } //\n");
    actor.addTag(std::string("control:") + char(1));
    actor.setTransform({{10, 20, 30}, Quaternion::fromAxisAngle({0, 0, 1}, std::numbers::pi / 2), {2, 3, 4}});
    actor.setTickEnabled(false);
    actor.rootComponent().setActive(false);
    auto& detached = actor.createComponent<SceneComponent>("Detached");
    CHECK(detached.attachTo(nullptr));
    detached.setLocalTransform({{100, 200, 300}});
    auto& child = actor.createComponent<SceneComponent>("Duplicate name");
    auto& parent = actor.createComponent<SceneComponent>("Duplicate name");
    CHECK(child.attachTo(&parent));
    parent.setLocalTransform({{1, 2, 3}, Quaternion::fromAxisAngle({0, 1, 0}, 0.3), {1, 2, 1}});
    child.setLocalTransform({{3, -2, 1}, Quaternion::fromAxisAngle({1, 0, 0}, -0.5), {-2, 0.25, 0}});
    auto& behavior = actor.createComponent<ActorComponent>("Behavior\nline");
    behavior.setActive(false);
    behavior.setTickEnabled(false);
    CHECK(actor.beginPlay());
    second.setPosition({-10, 0, 5});
    const std::array placements{UsdActorPlacement{&actor, "Player", {
        {nullptr, "./assets/model@v1.usda", "", true},
        {&parent, "./assets/shape.usda", "/Shape", false},
        {&detached, "./assets/model@v1.usda"}}}, UsdActorPlacement{&second, "Second"}};
    const auto layer = exportUsdLayer(placements);
    auto write = [&](const std::string& file, const std::string& text) {
        std::ofstream stream(directory / file, std::ios::binary);
        stream.exceptions(std::ios::badbit | std::ios::failbit);
        stream << text;
    };
    write("fixture.usda", layer.usda);
    write("fixture-y.usda", exportUsdLayer(placements, {"Scene", UsdUpAxis::Y, 0.01}).usda);
    write("fixture-empty.usda", exportUsdLayer({}).usda);
    write("player-name.txt", name);
    std::ofstream expected(directory / "points.tsv", std::ios::binary);
    expected.exceptions(std::ios::badbit | std::ios::failbit);
    expected.imbue(std::locale::classic());
    expected << std::setprecision(17);
    const std::array points{Vector3{}, Vector3{1, 2, 3}, Vector3{-4, 0.5, 2}};
    for (std::size_t a = 0; a < layer.actors.size(); ++a) {
        const auto components = placements[a].actor->components();
        for (const auto& binding : layer.actors[a].components) {
            const auto* scene = dynamic_cast<const SceneComponent*>(components[binding.componentIndex]);
            if (!scene) continue;
            for (const auto point : points) {
                const auto result = scene->worldMatrix().transformPoint(point);
                expected << binding.primPath << '\t' << point.x << '\t' << point.y << '\t' << point.z
                    << '\t' << result.x << '\t' << result.y << '\t' << result.z << '\n';
            }
        }
    }
    CHECK(actor.destroy());
}

int main(int argc, char** argv)
{
    try {
        if (argc < 2) throw std::runtime_error("Specify a USD test case");
        const std::string_view which = argv[1];
        if (which == "matrix") matrix();
        else if (which == "layer") layer();
        else if (which == "hierarchy") hierarchy();
        else if (which == "references") references();
        else if (which == "validation") validation();
        else if (which == "fixture" && argc == 3) fixture(argv[2]);
        else throw std::runtime_error("Unknown USD test case");
        std::cout << "USD " << which << " passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
