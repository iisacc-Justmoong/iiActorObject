#include <iiActorObject.h>

#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>

using namespace iiActorObject;
static_assert(__cplusplus > 202002L);
static_assert(!std::is_copy_constructible_v<Actor> && !std::is_move_constructible_v<Actor>);
static_assert(!std::is_copy_constructible_v<ActorComponent>);

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error( \
    std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #condition); } while (false)

template<class Exception, class F> void throws(F&& function)
{
    bool caught = false;
    try { function(); } catch (const Exception&) { caught = true; }
    CHECK(caught);
}

void near(Vector3 actual, Vector3 expected)
{
    CHECK(std::abs(actual.x - expected.x) < 1e-9);
    CHECK(std::abs(actual.y - expected.y) < 1e-9);
    CHECK(std::abs(actual.z - expected.z) < 1e-9);
}

using Events = std::vector<std::string>;
class ProbeActor : public Actor {
public:
    explicit ProbeActor(Events& events) : Actor("Actor"), events(events) {}
    Events& events;
    bool destroyOnBegin = false;
    int begins = 0, ticks = 0, ends = 0, destroys = 0;
    double elapsed = 0;
    EndPlayReason lastReason = EndPlayReason::Stopped;
private:
    void onBeginPlay() noexcept override
    {
        ++begins; events.push_back("A.begin");
        if (destroyOnBegin) destroy();
    }
    void onTick(double dt) noexcept override
    {
        ++ticks; elapsed += dt; events.push_back("A.tick");
    }
    void onEndPlay(EndPlayReason reason) noexcept override
    {
        ++ends; lastReason = reason; events.push_back("A.end");
    }
    void onDestroyed() noexcept override { ++destroys; events.push_back("A.destroy"); }
};

class ProbeComponent : public ActorComponent {
public:
    ProbeComponent(Events& events, std::string key, int* released = nullptr)
        : ActorComponent(key), events(events), key(std::move(key)), released(released) {}
    ~ProbeComponent() override { if (released) ++*released; }
    Events& events;
    std::string key;
    int* released;
    int begins = 0, ticks = 0, ends = 0;
    double elapsed = 0;
    bool destroyOnBegin = false, destroyOnTick = false, destroyOnEnd = false;
    bool checkMutation = false, mutationRejected = false, removeRejected = false;
    bool recursiveTickRejected = false, queuedDestroy = false, stillPlayingInHook = false;
    EndPlayReason lastReason = EndPlayReason::Stopped;
    EndPlayReason* reportedReason = nullptr;
private:
    void onBeginPlay() noexcept override
    {
        ++begins; events.push_back(key + ".begin");
        if (destroyOnBegin) owner()->destroy();
    }
    void onTick(double dt) noexcept override
    {
        ++ticks; elapsed += dt; events.push_back(key + ".tick");
        if (checkMutation) {
            recursiveTickRejected = !owner()->tick(0.0);
            removeRejected = !owner()->removeComponent(*this);
            try { owner()->createComponent<ActorComponent>(); }
            catch (const std::logic_error&) { mutationRejected = true; }
        }
        if (destroyOnTick) {
            queuedDestroy = owner()->destroy() && !owner()->destroy();
            stillPlayingInHook = owner()->state() == LifecycleState::Playing;
        }
    }
    void onEndPlay(EndPlayReason reason) noexcept override
    {
        ++ends; lastReason = reason; events.push_back(key + ".end");
        if (reportedReason) *reportedReason = reason;
        if (destroyOnEnd) owner()->destroy();
    }
};

void identity()
{
    CHECK(!static_cast<bool>(ActorId{}));
    std::unordered_set<std::uint64_t> ids;
    std::vector<std::unique_ptr<Actor>> actors;
    for (int i = 0; i < 128; ++i) {
        auto actor = std::make_unique<Actor>();
        CHECK(static_cast<bool>(actor->id()));
        CHECK(ids.insert(actor->id().value).second);
        CHECK(actor->componentCount() == 1);
        CHECK(actor->rootComponent().owner() == actor.get());
        CHECK(actor->rootComponent().parent() == nullptr);
        CHECK(actor->state() == LifecycleState::Created);
        near(actor->position(), {});
        CHECK((actor->transform().scale == Vector3{1, 1, 1}));
        actors.push_back(std::move(actor));
    }
    const auto& immutable = *actors.front();
    CHECK(immutable.components().front() == &immutable.rootComponent());
    CHECK(immutable.findComponent<SceneComponent>() == &immutable.rootComponent());
    CHECK(immutable.rootComponent().owner() == &immutable);
}

void properties()
{
    Actor actor("Original");
    const auto id = actor.id();
    actor.setName("Renamed");
    CHECK(actor.name() == "Renamed" && actor.id() == id);
    CHECK(actor.addTag("Player"));
    CHECK(!actor.addTag("Player") && !actor.addTag(""));
    CHECK(actor.addTag("Interactive") && actor.tags().size() == 2);
    CHECK(actor.hasTag("Player") && !actor.hasTag("player"));
    CHECK(actor.removeTag("Player") && !actor.removeTag("Player"));
    CHECK(actor.tags().front() == "Interactive");
    actor.rootComponent().setName("Anchor");
    CHECK(actor.rootComponent().name() == "Anchor");
    CHECK(actor.destroy() && actor.id() == id);
}

void transform()
{
    const double halfPi = std::numbers::pi / 2;
    Matrix4 identity;
    near(identity.transformPoint({1, 2, 3}), {1, 2, 3});
    Transform t{{10, 20, 30}, Quaternion::fromAxisAngle({0, 0, 2}, halfPi), {2, 3, 4}};
    near(t.matrix().transformPoint({1, 2, 3}), {4, 22, 42});
    near(t.matrix().transformVector({1, 2, 3}), {-6, 2, 12});
    CHECK(t.matrix().elements()[12] == 10);
    near(Transform{{}, {0, 0, 0, 50}}.matrix().transformPoint({3, 2, 1}), {3, 2, 1});
    const auto q = Quaternion{1e308, 0, 0, 1e308}.normalized();
    CHECK(std::isfinite(q.x) && std::abs(q.x - std::sqrt(0.5)) < 1e-9);
    near(Transform{{4, 5, 6}, {}, {0, 0, 0}}.matrix().transformPoint({8, 9, 10}), {4, 5, 6});
    near(Transform{{}, {}, {-1, 2, 3}}.matrix().transformPoint({2, 3, 4}), {-2, 6, 12});
    const Transform child{{3, 4, 5}, Quaternion::fromAxisAngle({1, 0, 0}, 0.7), {4, 2, 1}};
    const Vector3 p{2, 5, 7};
    near((t.matrix() * child.matrix()).transformPoint(p),
        t.matrix().transformPoint(child.matrix().transformPoint(p)));
    Actor actor;
    actor.setTransform(t);
    near(actor.rootComponent().worldPosition(), {10, 20, 30});
    actor.rootComponent().setLocalTransform(child);
    CHECK(actor.transform() == child.validated());
    actor.setPosition({100, 0, 0});
    CHECK(actor.transform().rotation == child.validated().rotation);
}

void scene()
{
    Actor actor;
    actor.setTransform({{10, 0, 0}, Quaternion::fromAxisAngle({0, 0, 1}, std::numbers::pi / 2), {2, 3, 4}});
    auto& parent = actor.createComponent<SceneComponent>("Parent");
    auto& child = actor.createComponent<SceneComponent>("Child");
    CHECK(parent.parent() == &actor.rootComponent());
    CHECK(child.attachTo(&parent));
    parent.setLocalTransform({{1, 0, 0}});
    child.setLocalTransform({{0, 2, 3}});
    near(child.worldPosition(), {4, 2, 12});
    CHECK(!parent.attachTo(&child) && !child.attachTo(&child));
    CHECK(!actor.rootComponent().attachTo(&child));
    Actor other;
    CHECK(!child.attachTo(&other.rootComponent()));
    CHECK(!actor.removeComponent(parent));
    const auto local = child.localTransform();
    CHECK(child.attachTo(nullptr));
    CHECK(child.localTransform() == local && parent.children().empty());
    near(child.worldPosition(), {0, 2, 3});
    CHECK(actor.removeComponent(parent));
    CHECK(child.attachTo(&actor.rootComponent()));
    CHECK(child.attachTo(&actor.rootComponent()));
    CHECK(actor.rootComponent().children().size() == 1);
    CHECK(actor.removeComponent(child) && actor.rootComponent().children().empty());
    SceneComponent detachedA, detachedB;
    CHECK(!detachedA.attachTo(&detachedB));
}

void components()
{
    Events events;
    int releases = 0;
    {
        ProbeActor actor(events);
        auto& first = actor.createComponent<ProbeComponent>(events, "C1", &releases);
        EndPlayReason removedReason = EndPlayReason::Stopped;
        first.reportedReason = &removedReason;
        CHECK(first.owner() == &actor && actor.componentCount() == 2);
        CHECK(actor.findComponent<ProbeComponent>() == &first);
        CHECK(actor.beginPlay());
        auto& second = actor.createComponent<ProbeComponent>(events, "C2", &releases);
        CHECK(second.begins == 1 && second.state() == LifecycleState::Playing);
        Actor foreign;
        CHECK(!foreign.removeComponent(first));
        CHECK(!actor.removeComponent(actor.rootComponent()));
        CHECK(actor.removeComponent(first) && releases == 1);
        CHECK(removedReason == EndPlayReason::Removed);
        CHECK(actor.findComponent<ProbeComponent>() == &second);
        CHECK((events == Events{"A.begin", "C1.begin", "C2.begin", "C1.end"}));
        CHECK(actor.destroy());
        CHECK(second.ends == 1 && second.lastReason == EndPlayReason::Destroyed);
        CHECK(second.state() == LifecycleState::Destroyed);
        CHECK(releases == 1); // destroy() never releases storage inside a callback.
    }
    CHECK(releases == 2);
    events.clear();
    {
        Actor actor;
        actor.createComponent<ProbeComponent>(events, "Unstarted");
    }
    CHECK(events.empty()); // Ordinary C++ destruction does not dispatch hooks.
}

void lifecycle()
{
    Events events;
    ProbeActor actor(events);
    auto& first = actor.createComponent<ProbeComponent>(events, "C1");
    auto& second = actor.createComponent<ProbeComponent>(events, "C2");
    CHECK(!actor.tick(0.1) && !actor.endPlay());
    first.setActive(false);
    CHECK(actor.beginPlay() && !actor.beginPlay());
    CHECK(actor.tick(0.25));
    CHECK(first.begins == 1 && first.ticks == 0 && second.ticks == 1);
    first.setActive(true);
    second.setTickEnabled(false);
    actor.setTickEnabled(false);
    CHECK(actor.tick(0.5));
    CHECK(actor.ticks == 1 && first.ticks == 1 && second.ticks == 1);
    CHECK(actor.endPlay() && !actor.endPlay() && !actor.tick(0));
    CHECK((events == Events{"A.begin", "C1.begin", "C2.begin", "A.tick", "C2.tick",
        "C1.tick", "C2.end", "C1.end", "A.end"}));
    CHECK(actor.state() == LifecycleState::Stopped && first.state() == LifecycleState::Stopped);
    CHECK(actor.beginPlay() && first.begins == 2);
    CHECK(actor.destroy() && !actor.destroy() && !actor.beginPlay());
    CHECK(actor.ends == 2 && actor.destroys == 1 && first.ends == 2);
    CHECK(actor.lastReason == EndPlayReason::Destroyed);
    CHECK(!actor.tick(1));
    Events unstartedEvents;
    ProbeActor unstarted(unstartedEvents);
    CHECK(unstarted.destroy());
    CHECK((unstartedEvents == Events{"A.destroy"}));
}

void callbacks()
{
    Events events;
    ProbeActor actor(events);
    auto& first = actor.createComponent<ProbeComponent>(events, "C1");
    auto& second = actor.createComponent<ProbeComponent>(events, "C2");
    first.checkMutation = first.destroyOnTick = true;
    CHECK(actor.beginPlay() && actor.tick(0.1));
    CHECK(actor.isDestroyed() && first.queuedDestroy && first.stillPlayingInHook);
    CHECK(first.mutationRejected && first.removeRejected && first.recursiveTickRejected);
    CHECK(second.ticks == 0 && second.ends == 1 && actor.destroys == 1);
    CHECK((events == Events{"A.begin", "C1.begin", "C2.begin", "A.tick", "C1.tick",
        "C2.end", "C1.end", "A.end", "A.destroy"}));
    Events earlyEvents;
    ProbeActor early(earlyEvents);
    early.destroyOnBegin = true;
    auto& never = early.createComponent<ProbeComponent>(earlyEvents, "Never");
    CHECK(early.beginPlay() && early.isDestroyed());
    CHECK(never.begins == 0 && never.ends == 0 && never.state() == LifecycleState::Destroyed);
    CHECK((earlyEvents == Events{"A.begin", "A.end", "A.destroy"}));
    Events beginEvents;
    ProbeActor beginActor(beginEvents);
    auto& beginning = beginActor.createComponent<ProbeComponent>(beginEvents, "Begin");
    beginning.destroyOnBegin = true;
    auto& later = beginActor.createComponent<ProbeComponent>(beginEvents, "Later");
    CHECK(beginActor.beginPlay());
    CHECK(beginActor.isDestroyed() && beginning.ends == 1 && later.begins == 0);
    Events endEvents;
    ProbeActor endActor(endEvents);
    auto& ending = endActor.createComponent<ProbeComponent>(endEvents, "End");
    ending.destroyOnEnd = true;
    CHECK(endActor.beginPlay() && endActor.endPlay());
    CHECK(endActor.isDestroyed() && ending.ends == 1 && endActor.ends == 1 && endActor.destroys == 1);
    Events removeEvents;
    ProbeActor removeActor(removeEvents);
    auto& removing = removeActor.createComponent<ProbeComponent>(removeEvents, "Remove");
    removing.destroyOnEnd = true;
    CHECK(removeActor.beginPlay() && removeActor.removeComponent(removing));
    CHECK(removeActor.isDestroyed() && removeActor.componentCount() == 1);
    class DestroyOnStart final : public ActorComponent {
        void onBeginPlay() noexcept override { owner()->destroy(); }
    };
    Actor running;
    CHECK(running.beginPlay());
    auto& added = running.createComponent<DestroyOnStart>();
    CHECK(running.isDestroyed() && added.state() == LifecycleState::Destroyed);
    CHECK(added.owner() == &running); // Returned reference stays alive after logical destruction.
}

void validation()
{
    Actor actor;
    throws<std::invalid_argument>([&] { actor.addComponent(nullptr); });
    const auto before = actor.transform();
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto inf = std::numeric_limits<double>::infinity();
    throws<std::invalid_argument>([&] { actor.setPosition({nan, 0, 0}); });
    throws<std::invalid_argument>([&] { actor.setTransform({{}, {}, {inf, 1, 1}}); });
    throws<std::invalid_argument>([&] { actor.setTransform({{}, {0, 0, 0, 0}}); });
    throws<std::invalid_argument>([&] { (void)Quaternion::fromAxisAngle({}, 1); });
    throws<std::invalid_argument>([&] { (void)Quaternion::fromAxisAngle({0, 1, 0}, inf); });
    throws<std::invalid_argument>([&] { (void)Quaternion{nan, 0, 0, 1}.normalized(); });
    CHECK(actor.transform() == before);
    CHECK(actor.beginPlay());
    throws<std::invalid_argument>([&] { actor.tick(-0.1); });
    throws<std::invalid_argument>([&] { actor.tick(nan); });
    throws<std::invalid_argument>([&] { actor.tick(inf); });
    CHECK(actor.tick(0));
    CHECK(actor.destroy());
    throws<std::logic_error>([&] { actor.createComponent<ActorComponent>(); });
    CHECK(!actor.removeComponent(actor.rootComponent()));
}

int main(int argc, char** argv)
{
    try {
        if (argc != 2) throw std::runtime_error("Specify a test case");
        const std::string_view which = argv[1];
        if (which == "identity") identity();
        else if (which == "properties") properties();
        else if (which == "transform") transform();
        else if (which == "scene") scene();
        else if (which == "components") components();
        else if (which == "lifecycle") lifecycle();
        else if (which == "callbacks") callbacks();
        else if (which == "validation") validation();
        else throw std::runtime_error("Unknown test case");
        std::cout << which << " passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
