#pragma once
#include <iiActorObject/Object3DAsset.h>

namespace iiActorObject
{
class Object3DEditor;
class Object3DReference;
class Object3DRigSession;
class Object3DRigDriver;
class Object3DTemplate;
struct Object3DTemplateDefinition;
struct TemplateValue;
namespace object3d_detail
{
struct State;
struct AssetReference;
struct EditorReference;
struct RigReference;
struct TemplateReference;
struct ProjectAccess;
struct RigSamplingAccess;
struct TemplateMorphBakeAccess;
} // namespace object3d_detail
using Object3DId = std::uint64_t;

// A reference token, not an owner of the asset or engine. Explicit engine erasure
// and engine destruction invalidate it. Lookups then throw std::logic_error.
class IIACTOROBJECT_EXPORT Object3DHandle final
{
  public:
    Object3DHandle() = default;
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] Object3DId id() const noexcept;
    [[nodiscard]] const Object3DAsset &get() const;
    [[nodiscard]] Object3DReference engine() const;
    bool operator==(const Object3DHandle &) const noexcept = default;

  private:
    friend class Object3DReference;
    explicit Object3DHandle(std::shared_ptr<const object3d_detail::AssetReference> reference);
    std::shared_ptr<const object3d_detail::AssetReference> reference_;
};
class IIACTOROBJECT_EXPORT Object3DEditorHandle final
{
  public:
    Object3DEditorHandle() = default;
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] Object3DId id() const noexcept;
    [[nodiscard]] Object3DEditor &get() const;

  private:
    friend class Object3DReference;
    explicit Object3DEditorHandle(std::shared_ptr<const object3d_detail::EditorReference> reference);
    std::shared_ptr<const object3d_detail::EditorReference> reference_;
};
struct Object3DEngineLimits final
{
    std::size_t assets = 4096;
    std::size_t editors = 256;
    std::size_t rigs = 256;
    std::size_t templates = 64;
};

class IIACTOROBJECT_EXPORT Object3DTemplateHandle final
{
  public:
    Object3DTemplateHandle()=default;
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] Object3DId id() const noexcept;
    [[nodiscard]] const Object3DTemplate &get() const;
  private:
    friend class Object3DReference;
    explicit Object3DTemplateHandle(std::shared_ptr<const object3d_detail::TemplateReference> reference);
    std::shared_ptr<const object3d_detail::TemplateReference> reference_;
};

class IIACTOROBJECT_EXPORT Object3DRigHandle final
{
  public:
    Object3DRigHandle() = default;
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] Object3DId id() const noexcept;
    [[nodiscard]] Object3DRigSession &get() const;
  private:
    friend class Object3DReference;
    explicit Object3DRigHandle(std::shared_ptr<const object3d_detail::RigReference> reference);
    std::shared_ptr<const object3d_detail::RigReference> reference_;
};

// Borrowed access to one core engine. Does not prolong its lifetime.
class IIACTOROBJECT_EXPORT Object3DReference final
{
  public:
    Object3DReference() = default;
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool owns(const Object3DHandle &object) const noexcept;
    [[nodiscard]] Object3DHandle createObject(Object3DData data) const;
    [[nodiscard]] Object3DHandle findObject(Object3DId id) const;
    [[nodiscard]] Object3DEditorHandle editObject(const Object3DHandle &object,
                                                  std::size_t historyLimit = 32) const;
    [[nodiscard]] Object3DEditorHandle editTemplate(const Object3DTemplateHandle &program,
        std::span<const TemplateValue> values={},std::size_t historyLimit=32) const;
    bool eraseObject(Object3DId id) const;
    bool eraseEditor(Object3DId id) const;
    [[nodiscard]] Object3DRigHandle createRigSession(const Object3DHandle &object,
        std::unique_ptr<Object3DRigDriver> driver, std::span<const BoneIndex> jointMapping = {}) const;
    bool eraseRig(Object3DId id) const;
    [[nodiscard]] Object3DTemplateHandle createTemplate(Object3DTemplateDefinition definition) const;
    bool eraseTemplate(Object3DId id) const;
    // The core reclaims entries for which no reference tokens remain.
    std::size_t collectGarbage() const;
    [[nodiscard]] std::size_t objectCount() const;
    [[nodiscard]] std::size_t editorCount() const;
    [[nodiscard]] std::size_t rigCount() const;
    [[nodiscard]] std::size_t templateCount() const;

  private:
    friend class Object3DEngine;
    friend class Object3DHandle;
    friend struct object3d_detail::ProjectAccess;
    explicit Object3DReference(std::weak_ptr<object3d_detail::State> state);
    std::weak_ptr<object3d_detail::State> state_;
};

// The sole long-lived owner of common 3D assets and editing sessions. No dependency
// on NSCharacter or other feature namespaces. One host thread serializes access.
class IIACTOROBJECT_EXPORT Object3DEngine final
{
  public:
    explicit Object3DEngine(Object3DEngineLimits limits = {});
    ~Object3DEngine();
    Object3DEngine(const Object3DEngine &) = delete;
    Object3DEngine &operator=(const Object3DEngine &) = delete;
    Object3DEngine(Object3DEngine &&) noexcept;
    Object3DEngine &operator=(Object3DEngine &&) noexcept;
    [[nodiscard]] Object3DReference reference() const noexcept;
    [[nodiscard]] Object3DHandle createObject(Object3DData data);
    [[nodiscard]] Object3DEditorHandle editObject(const Object3DHandle &object,
                                                  std::size_t historyLimit = 32);
    [[nodiscard]] Object3DEditorHandle editTemplate(const Object3DTemplateHandle &program,
        std::span<const TemplateValue> values={},std::size_t historyLimit=32);
    bool eraseObject(Object3DId id);
    bool eraseEditor(Object3DId id);
    [[nodiscard]] Object3DRigHandle createRigSession(const Object3DHandle &object,
        std::unique_ptr<Object3DRigDriver> driver, std::span<const BoneIndex> jointMapping = {});
    bool eraseRig(Object3DId id);
    [[nodiscard]] Object3DTemplateHandle createTemplate(Object3DTemplateDefinition definition);
    bool eraseTemplate(Object3DId id);
    std::size_t collectGarbage();
    [[nodiscard]] std::size_t objectCount() const;
    [[nodiscard]] std::size_t editorCount() const;
    [[nodiscard]] std::size_t rigCount() const;
    [[nodiscard]] std::size_t templateCount() const;

  private:
    std::shared_ptr<object3d_detail::State> state_;
};
} // namespace iiActorObject
