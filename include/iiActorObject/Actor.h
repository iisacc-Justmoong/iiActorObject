#pragma once
#include <iiActorObject/SceneComponent.h>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace iiActorObject {
struct ActorId {
    std::uint64_t value = 0;
    constexpr explicit operator bool() const noexcept { return value != 0; }
    constexpr bool operator==(const ActorId&) const = default;
};

class IIACTOROBJECT_EXPORT Actor {
public:
    explicit Actor(std::string name = {});
    // Host calls destroy() before deleting a running derived Actor.
    // Destructors release memory without calling lifecycle hooks.
    virtual ~Actor();
    Actor(const Actor&) = delete;
    Actor& operator=(const Actor&) = delete;
    Actor(Actor&&) = delete;
    Actor& operator=(Actor&&) = delete;
    [[nodiscard]] ActorId id() const noexcept;
    [[nodiscard]] const std::string& name() const noexcept;
    void setName(std::string name);
    bool addTag(std::string tag);
    bool removeTag(std::string_view tag);
    [[nodiscard]] bool hasTag(std::string_view tag) const noexcept;
    [[nodiscard]] std::span<const std::string> tags() const noexcept;
    [[nodiscard]] SceneComponent& rootComponent() noexcept;
    [[nodiscard]] const SceneComponent& rootComponent() const noexcept;
    [[nodiscard]] const Transform& transform() const noexcept;
    void setTransform(Transform transform);
    [[nodiscard]] Vector3 position() const noexcept;
    void setPosition(Vector3 position);
    // Actor owns components; ownership changes are forbidden inside hooks.
    ActorComponent& addComponent(std::unique_ptr<ActorComponent> component);
    template<std::derived_from<ActorComponent> T, class... Args>
    T& createComponent(Args&&... args)
    {
        ensureMutable();
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        auto& result = *component;
        addComponent(std::move(component));
        return result;
    }
    bool removeComponent(ActorComponent& component) noexcept;
    [[nodiscard]] std::size_t componentCount() const noexcept;
    [[nodiscard]] std::vector<ActorComponent*> components();
    [[nodiscard]] std::vector<const ActorComponent*> components() const;
    template<std::derived_from<ActorComponent> T>
    [[nodiscard]] T* findComponent() noexcept
    {
        for (const auto& component : components_)
            if (auto* found = dynamic_cast<T*>(component.get())) return found;
        return nullptr;
    }
    template<std::derived_from<ActorComponent> T>
    [[nodiscard]] const T* findComponent() const noexcept
    {
        for (const auto& component : components_)
            if (auto* found = dynamic_cast<const T*>(component.get())) return found;
        return nullptr;
    }
    [[nodiscard]] LifecycleState state() const noexcept;
    [[nodiscard]] bool isDestroyed() const noexcept;
    [[nodiscard]] bool isTickEnabled() const noexcept;
    void setTickEnabled(bool enabled) noexcept;
    bool beginPlay() noexcept;
    bool tick(double deltaSeconds);
    bool endPlay(EndPlayReason reason = EndPlayReason::Stopped) noexcept;
    // Logical destruction, deferred to dispatch completion when called in a hook.
    // Storage remains alive until its external owner deletes it.
    bool destroy() noexcept;
protected:
    virtual void onBeginPlay() noexcept;
    virtual void onTick(double deltaSeconds) noexcept;
    virtual void onEndPlay(EndPlayReason reason) noexcept;
    virtual void onDestroyed() noexcept;
private:
    void ensureMutable() const;
    void finishDispatch() noexcept;
    ActorId id_;
    std::string name_;
    std::vector<std::string> tags_;
    std::vector<std::unique_ptr<ActorComponent>> components_;
    SceneComponent* root_ = nullptr;
    LifecycleState state_ = LifecycleState::Created;
    bool tickEnabled_ = true;
    bool dispatching_ = false;
    bool destroyRequested_ = false;
};
} // namespace iiActorObject
