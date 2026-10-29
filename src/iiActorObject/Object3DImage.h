#pragma once
#include <iiActorObject/Export.h>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace iiActorObject
{
// An immutable encoded resource. Copies share bytes; the feature layer borrows
// it through the core-owned asset. Pixel decoding remains a host/codec concern.
class IIACTOROBJECT_EXPORT Object3DImage final
{
  public:
    Object3DImage()=default;
    Object3DImage(std::string name,std::vector<std::uint8_t> encoded,std::string mimeType="image/png");
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::string_view name() const noexcept;
    [[nodiscard]] std::string_view mimeType() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> encoded() const noexcept;
    [[nodiscard]] std::uint32_t width() const noexcept;
    [[nodiscard]] std::uint32_t height() const noexcept;
    [[nodiscard]] bool operator==(const Object3DImage &other) const noexcept;
  private:
    struct Data;
    std::shared_ptr<const Data> data_;
};
} // namespace iiActorObject
