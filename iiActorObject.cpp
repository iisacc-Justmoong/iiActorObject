#include "iiActorObject.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace iiActorObject {
namespace {
ActorId nextId()
{
    static std::atomic<std::uint64_t> next{1};
    auto value = next.load(std::memory_order_relaxed);
    for (;;) {
        if (value == std::numeric_limits<std::uint64_t>::max())
            throw std::overflow_error("Actor identifier space exhausted");
        if (next.compare_exchange_weak(value, value + 1, std::memory_order_relaxed)) return {value};
    }
}
}

Actor::Actor(std::string name) : id_(nextId()), name_(std::move(name))
{
    auto root = std::make_unique<SceneComponent>("Root");
    root_ = root.get();
    root_->owner_ = this;
    components_.push_back(std::move(root));
}
Actor::~Actor() = default;
ActorId Actor::id() const noexcept { return id_; }
const std::string& Actor::name() const noexcept { return name_; }
void Actor::setName(std::string name) { name_ = std::move(name); }
bool Actor::hasTag(std::string_view tag) const noexcept
{
    return std::find(tags_.begin(), tags_.end(), tag) != tags_.end();
}
bool Actor::addTag(std::string tag)
{
    if (tag.empty() || hasTag(tag)) return false;
    tags_.push_back(std::move(tag));
    return true;
}
bool Actor::removeTag(std::string_view tag)
{
    const auto found = std::find(tags_.begin(), tags_.end(), tag);
    if (found == tags_.end()) return false;
    tags_.erase(found);
    return true;
}
std::span<const std::string> Actor::tags() const noexcept { return tags_; }
SceneComponent& Actor::rootComponent() noexcept { return *root_; }
const SceneComponent& Actor::rootComponent() const noexcept { return *root_; }
const Transform& Actor::transform() const noexcept { return root_->localTransform(); }
void Actor::setTransform(Transform transform) { root_->setLocalTransform(transform); }
Vector3 Actor::position() const noexcept { return transform().position; }
void Actor::setPosition(Vector3 position)
{
    auto value = transform();
    value.position = position;
    setTransform(value);
}

void Actor::ensureMutable() const
{
    if (isDestroyed()) throw std::logic_error("Cannot add components to a destroyed Actor");
    if (dispatching_) throw std::logic_error("Cannot change component ownership inside lifecycle hooks");
}

ActorComponent& Actor::addComponent(std::unique_ptr<ActorComponent> component)
{
    ensureMutable();
    if (!component || component->owner_ || component->state_ != LifecycleState::Created)
        throw std::invalid_argument("Component must be a new, unowned object");
    auto& result = *component;
    components_.push_back(std::move(component));
    result.owner_ = this;
    try {
        if (auto* scene = dynamic_cast<SceneComponent*>(&result)) scene->attachTo(root_);
    } catch (...) {
        result.owner_ = nullptr;
        components_.pop_back();
        throw;
    }
    if (state_ == LifecycleState::Playing) {
        dispatching_ = true;
        result.beginPlay();
        finishDispatch();
    }
    return result;
}

bool Actor::removeComponent(ActorComponent& component) noexcept
{
    if (dispatching_ || isDestroyed() || &component == root_) return false;
    const auto found = std::find_if(components_.begin(), components_.end(),
        [&](const auto& item) { return item.get() == &component; });
    if (found == components_.end()) return false;
    if (const auto* scene = dynamic_cast<const SceneComponent*>(&component); scene && !scene->children().empty())
        return false;
    dispatching_ = true;
    component.endPlay(EndPlayReason::Removed);
    component.state_ = LifecycleState::Destroyed;
    component.owner_ = nullptr;
    components_.erase(found);
    finishDispatch();
    return true;
}
std::size_t Actor::componentCount() const noexcept { return components_.size(); }
std::vector<ActorComponent*> Actor::components()
{
    std::vector<ActorComponent*> result;
    result.reserve(components_.size());
    for (const auto& component : components_) result.push_back(component.get());
    return result;
}
std::vector<const ActorComponent*> Actor::components() const
{
    std::vector<const ActorComponent*> result;
    result.reserve(components_.size());
    for (const auto& component : components_) result.push_back(component.get());
    return result;
}
LifecycleState Actor::state() const noexcept { return state_; }
bool Actor::isDestroyed() const noexcept { return state_ == LifecycleState::Destroyed; }
bool Actor::isTickEnabled() const noexcept { return tickEnabled_; }
void Actor::setTickEnabled(bool enabled) noexcept { tickEnabled_ = enabled; }
void Actor::onBeginPlay() noexcept {}
void Actor::onTick(double) noexcept {}
void Actor::onEndPlay(EndPlayReason) noexcept {}
void Actor::onDestroyed() noexcept {}

void Actor::finishDispatch() noexcept
{
    dispatching_ = false;
    if (std::exchange(destroyRequested_, false)) destroy();
}

bool Actor::beginPlay() noexcept
{
    if (dispatching_ || isDestroyed() || state_ == LifecycleState::Playing) return false;
    state_ = LifecycleState::Playing;
    dispatching_ = true;
    onBeginPlay();
    for (const auto& component : components_) {
        if (destroyRequested_) break;
        component->beginPlay();
    }
    finishDispatch();
    return true;
}

bool Actor::tick(double deltaSeconds)
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0)
        throw std::invalid_argument("Frame delta must be finite and nonnegative");
    if (dispatching_ || state_ != LifecycleState::Playing) return false;
    dispatching_ = true;
    if (tickEnabled_) onTick(deltaSeconds);
    for (const auto& component : components_) {
        if (destroyRequested_) break;
        component->tick(deltaSeconds);
    }
    finishDispatch();
    return true;
}

bool Actor::endPlay(EndPlayReason reason) noexcept
{
    if (dispatching_ || state_ != LifecycleState::Playing) return false;
    state_ = LifecycleState::Stopped;
    dispatching_ = true;
    for (auto component = components_.rbegin(); component != components_.rend(); ++component)
        (*component)->endPlay(reason);
    onEndPlay(reason);
    finishDispatch();
    return true;
}

bool Actor::destroy() noexcept
{
    if (isDestroyed() || destroyRequested_) return false;
    if (dispatching_) {
        destroyRequested_ = true;
        return true;
    }
    const bool wasPlaying = state_ == LifecycleState::Playing;
    state_ = LifecycleState::Destroyed;
    dispatching_ = true;
    for (auto component = components_.rbegin(); component != components_.rend(); ++component) {
        if (wasPlaying) (*component)->endPlay(EndPlayReason::Destroyed);
        (*component)->state_ = LifecycleState::Destroyed;
    }
    if (wasPlaying) onEndPlay(EndPlayReason::Destroyed);
    onDestroyed();
    dispatching_ = false;
    return true;
}
} // namespace iiActorObject
