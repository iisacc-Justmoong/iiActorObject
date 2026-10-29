#include "../src/Interchange/GLB/Object3DGlbMeshAttributes.h"
#include <iiActorObject/Object3DInterchange.h>
#include <iiJson/Json.h>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail::glb_import;

namespace
{
Value parse(std::string_view text)
{
    auto result = ii::json::parse(text);
    if (!result) throw std::runtime_error("invalid test JSON");
    return std::move(*result);
}
void check(bool condition)
{
    if (!condition) throw std::runtime_error("GLB attribute contract failed");
}
template<class F> void rejects(F operation)
{
    try { operation(); }
    catch (const std::invalid_argument &) { return; }
    throw std::runtime_error("invalid GLB attributes were accepted");
}
}
int main()
{
    GlbImportOptions options;
    std::vector<std::string> warnings;
    const auto decoded = attributes(parse(R"({"POSITION":7,"NORMAL":1,"TEXCOORD_0":2,"TEXCOORD_1":3,"JOINTS_0":4,"WEIGHTS_0":5})"), options, warnings);
    check(decoded.position == 7 && decoded.normal == 1 && decoded.uv == std::vector<std::size_t>({2,3}));
    check(decoded.joints == std::vector<std::size_t>({4}) && decoded.weights == std::vector<std::size_t>({5}));
    rejects([&] { (void)attributes(parse(R"({"NORMAL":1})"), options, warnings); });
    rejects([&] { (void)attributes(parse(R"({"POSITION":7,"TEXCOORD_01":2})"), options, warnings); });
    rejects([&] { (void)attributes(parse(R"({"POSITION":7,"TEXCOORD_1":2})"), options, warnings); });
    rejects([&] { (void)attributes(parse(R"({"POSITION":7,"JOINTS_0":4})"), options, warnings); });
    rejects([&] { (void)attributes(parse(R"({"POSITION":7,"COLOR_0":1})"), options, warnings); });
    options.allowDiscardUnsupportedAttributes = true;
    (void)attributes(parse(R"({"POSITION":7,"COLOR_0":1})"), options, warnings);
    check(!warnings.empty());
    auto parsedTargets = targets(parse(R"({"targets":[{"POSITION":8,"NORMAL":9}]})"), options, warnings);
    check(parsedTargets.size() == 1 && parsedTargets[0].position == 8 && parsedTargets[0].normal == 9);
    rejects([&] { (void)targets(parse(R"({"targets":[{}]})"), options, warnings); });
    auto differentLayout = decoded;
    differentLayout.normal = 10;
    check(bundleKey(decoded, parsedTargets) != bundleKey(differentLayout, parsedTargets));
    check(pointKey(decoded, parsedTargets) == pointKey(differentLayout, parsedTargets));
    auto differentPoint = parsedTargets;
    differentPoint[0].position = 11;
    check(pointKey(decoded, parsedTargets) != pointKey(decoded, differentPoint));
    const auto mesh = parse(R"({"extras":{"targetNames":["","Smile","Smile"]},"weights":[0,0.1,0.2]})");
    const auto node = parse(R"({"weights":[0.3,0.4,0.5]})");
    const auto names = morphNames(mesh, 3, warnings);
    check(names == std::vector<std::string>({"Target_0", "Smile", "Smile_1"}));
    check(initialWeights(mesh, node, 3) == std::vector<double>({0.3,0.4,0.5}));
    check(initialWeights(mesh, parse("{}"), 3) == std::vector<double>({0,0.1,0.2}));
    rejects([&] { (void)morphNames(mesh, 2, warnings); });
    rejects([&] { (void)initialWeights(mesh, node, 2); });
}
