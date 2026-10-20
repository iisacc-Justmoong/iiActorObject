#include <iiActorObject.h>
#include <array>
#include <cmath>
#include <iostream>
#include <type_traits>

static_assert(__cplusplus > 202002L, "Installed target must propagate C++23");
static_assert(!std::is_copy_constructible_v<iiActorObject::Actor>);
class Counter final : public iiActorObject::ActorComponent {
public:
    int frames = 0;
private:
    void onTick(double) noexcept override { ++frames; }
};
int main()
{
    using namespace iiActorObject;
    Actor actor("Installed actor");
    actor.setPosition({10, 0, 0});
    auto& mesh = actor.createComponent<SceneComponent>("Mesh anchor");
    mesh.setLocalTransform({{2, 3, 4}});
    auto& counter = actor.createComponent<Counter>();
    if (!actor.beginPlay() || !actor.tick(1.0 / 60.0) || counter.frames != 1) return 1;
    if (std::abs(mesh.worldPosition().x - 12.0) > 1e-9) return 2;
    const std::array placements{UsdActorPlacement{&actor, "Player"}};
    const auto layer = exportUsdLayer(placements);
    if (layer.actors[0].components[1].primPath != "/World/Player/Component_1") return 4;
    if (layer.usda.find("def Xform \"Player\"") == std::string::npos) return 5;
    if (toUsdMatrix(actor.transform().matrix())[3][0] != 10) return 6;
    if (!actor.destroy()) return 7;
    if (!actor.isDestroyed() || actor.tick(1.0 / 60.0)) return 3;
    std::cout << "Installed C++23 Actor/component/lifecycle/USD passed\n";
}
