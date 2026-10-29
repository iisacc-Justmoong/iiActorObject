#pragma once

#include <iiActorObject/Export.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace iiActorObject {

enum class GlbComponentType : std::uint32_t {
    Byte = 5120, UnsignedByte = 5121, Short = 5122,
    UnsignedShort = 5123, UnsignedInt = 5125, Float = 5126
};
enum class GlbAccessorType { Scalar, Vec2, Vec3, Vec4, Mat2, Mat3, Mat4 };

struct GlbReadOptions final {
    std::size_t maximumBytes = 512U * 1024U * 1024U;
    std::size_t maximumJsonBytes = 32U * 1024U * 1024U;
    std::size_t maximumJsonItems = 1000000;
    std::size_t maximumBufferViews = 100000;
    std::size_t maximumAccessors = 100000;
    std::size_t maximumAccessorValues = 64U * 1024U * 1024U;
    std::size_t maximumSparseEntries = 8U * 1024U * 1024U;
};

struct GlbAccessorInfo final {
    std::size_t count;
    std::size_t components;
    GlbComponentType component;
    GlbAccessorType type;
    bool normalized;
};

// An immutable, owning GLB container and accessor decoder. It performs no file I/O.
// Parse validates buffer bounds, layouts and sparse indices; accessor methods
// throw for invalid indices, incompatible types or non-finite floating values.
class IIACTOROBJECT_EXPORT GlbDocument final {
public:
    [[nodiscard]] static GlbDocument parse(std::span<const std::uint8_t> bytes,
                                           const GlbReadOptions& options = {});
    [[nodiscard]] std::size_t accessorCount() const noexcept;
    [[nodiscard]] std::size_t bufferViewCount() const noexcept;
    // The returned span remains valid for the lifetime of this document or a copy.
    [[nodiscard]] std::span<const std::uint8_t> bufferView(std::size_t index) const;
    [[nodiscard]] GlbAccessorInfo accessor(std::size_t index) const;
    [[nodiscard]] std::vector<double> readFloats(std::size_t index) const;
    [[nodiscard]] std::vector<std::uint32_t> readUnsigned(std::size_t index) const;
    // The returned view remains valid for the lifetime of this document or a copy.
    [[nodiscard]] std::string_view json() const noexcept;

private:
    struct Impl;
    explicit GlbDocument(std::shared_ptr<const Impl> impl) : impl_(std::move(impl)) {}
    std::shared_ptr<const Impl> impl_;
};

} // namespace iiActorObject
