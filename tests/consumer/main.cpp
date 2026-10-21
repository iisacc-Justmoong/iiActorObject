#include <array>
#include <cmath>
#include <iiActorObject.h>
#include <iostream>
#include <type_traits>

static_assert(__cplusplus > 202002L, "Installed target must propagate C++23");
static_assert(!std::is_copy_constructible_v<iiActorObject::Actor>);
class Counter final : public iiActorObject::ActorComponent
{
  public:
    int frames = 0;

  private:
    void onTick(double) noexcept override
    {
        ++frames;
    }
};
int main()
{
    using namespace iiActorObject;
    Actor actor("Installed actor");
    actor.setPosition({10, 0, 0});
    auto &mesh = actor.createComponent<SceneComponent>("Mesh anchor");
    mesh.setLocalTransform({{2, 3, 4}});
    auto &counter = actor.createComponent<Counter>();
    if (!actor.beginPlay() || !actor.tick(1.0 / 60.0) || counter.frames != 1)
        return 1;
    if (std::abs(mesh.worldPosition().x - 12.0) > 1e-9)
        return 2;
    const std::array placements{UsdActorPlacement{&actor, "Player"}};
    const auto layer = exportUsdLayer(placements);
    if (layer.actors[0].components[1].primPath != "/World/Player/Component_1")
        return 4;
    if (layer.usda.find("def Xform \"Player\"") == std::string::npos)
        return 5;
    if (toUsdMatrix(actor.transform().matrix())[3][0] != 10)
        return 6;
    if (!actor.destroy())
        return 7;
    if (!actor.isDestroyed() || actor.tick(1.0 / 60.0))
        return 3;
    auto skeleton = std::make_shared<Skeleton>(std::vector<Bone>{{"root", NoBone, {}}});
    SkeletalMeshLod lod;
    lod.vertices = {{{1, 0, 0}, {0, 0, 1}, {{0, 1}}}};
    lod.morphTargets = {{"smile", {{0, {0, 0, 1}, {}}}}};
    auto skin = std::make_shared<SkeletalMesh>(skeleton, std::vector<SkeletalMeshLod>{lod});
    auto clip = std::make_shared<AnimationClip>(skeleton, 1,
                                                std::vector<BoneTrack>{{0, {{0, {}}, {1, {{1, 0, 0}}}}}});
    iiCharacter character(skeleton, "Installed character");
    auto &body = character.addMesh(skin);
    character.animation().play(clip);
    character.beginPlay();
    character.tick(.5);
    if (std::abs(body.deformedVertices()[0].position.x - 1.5) > 1e-9)
        return 8;
    FacialRig face(skeleton, {"smile"}, {}, {{"smile", {{0, NoFacialControl, 1, 0}}}});
    std::array<double, 1> control{.75}, shapes{};
    face.evaluate(character.pose(), control, shapes, {});
    body.setMorphWeight(0, shapes[0]);
    if (std::abs(body.deformedVertices()[0].position.z - .75) > 1e-9)
        return 9;
    const auto count = body.deformationCount();
    (void)body.deformedVertices();
    if (body.deformationCount() != count)
        return 10;
    character.destroy();
    std::cout << "Installed C++23 Actor/character/animation/skin/facial rig/USD passed\n";
}
