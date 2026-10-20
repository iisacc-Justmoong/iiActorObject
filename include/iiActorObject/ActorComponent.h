#pragma once
#include <iiActorObject/Export.h>
#include <string>

namespace iiActorObject {
class Actor;
enum class LifecycleState { Created, Playing, Stopped, Destroyed };
enum class EndPlayReason { Stopped, Destroyed, Removed };

class IIACTOROBJECT_EXPORT ActorComponent {
public:
    explicit ActorComponent(std::string name = {});
    virtual ~ActorComponent();
    ActorComponent(const ActorComponent&) = delete;
    ActorComponent& operator=(const ActorComponent&) = delete;
    ActorComponent(ActorComponent&&) = delete;
    ActorComponent& operator=(ActorComponent&&) = delete;
    [[nodiscard]] Actor* owner() noexcept;
    [[nodiscard]] const Actor* owner() const noexcept;
    [[nodiscard]] const std::string& name() const noexcept;
    void setName(std::string name);
    [[nodiscard]] LifecycleState state() const noexcept;
    [[nodiscard]] bool isActive() const noexcept;
    void setActive(bool active) noexcept;
    [[nodiscard]] bool isTickEnabled() const noexcept;
    void setTickEnabled(bool enabled) noexcept;
protected:
    // Hooks run synchronously on the host thread; exceptions must not escape.
    virtual void onBeginPlay() noexcept;
    virtual void onTick(double deltaSeconds) noexcept;
    virtual void onEndPlay(EndPlayReason reason) noexcept;
private:
    friend class Actor;
    void beginPlay() noexcept;
    void tick(double deltaSeconds) noexcept;
    void endPlay(EndPlayReason reason) noexcept;
    Actor* owner_ = nullptr;
    std::string name_;
    LifecycleState state_ = LifecycleState::Created;
    bool active_ = true;
    bool tickEnabled_ = true;
};
} // namespace iiActorObject
