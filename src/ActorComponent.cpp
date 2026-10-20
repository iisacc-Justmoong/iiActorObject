#include <iiActorObject/ActorComponent.h>

#include <utility>

namespace iiActorObject {
ActorComponent::ActorComponent(std::string name) : name_(std::move(name)) {}
ActorComponent::~ActorComponent() = default;
Actor* ActorComponent::owner() noexcept { return owner_; }
const Actor* ActorComponent::owner() const noexcept { return owner_; }
const std::string& ActorComponent::name() const noexcept { return name_; }
void ActorComponent::setName(std::string name) { name_ = std::move(name); }
LifecycleState ActorComponent::state() const noexcept { return state_; }
bool ActorComponent::isActive() const noexcept { return active_; }
void ActorComponent::setActive(bool active) noexcept { active_ = active; }
bool ActorComponent::isTickEnabled() const noexcept { return tickEnabled_; }
void ActorComponent::setTickEnabled(bool enabled) noexcept { tickEnabled_ = enabled; }
void ActorComponent::onBeginPlay() noexcept {}
void ActorComponent::onTick(double) noexcept {}
void ActorComponent::onEndPlay(EndPlayReason) noexcept {}

void ActorComponent::beginPlay() noexcept
{
    if (state_ == LifecycleState::Playing || state_ == LifecycleState::Destroyed) return;
    state_ = LifecycleState::Playing;
    onBeginPlay();
}

void ActorComponent::tick(double deltaSeconds) noexcept
{
    if (state_ == LifecycleState::Playing && active_ && tickEnabled_) onTick(deltaSeconds);
}

void ActorComponent::endPlay(EndPlayReason reason) noexcept
{
    if (state_ != LifecycleState::Playing) return;
    state_ = LifecycleState::Stopped;
    onEndPlay(reason);
}
} // namespace iiActorObject
