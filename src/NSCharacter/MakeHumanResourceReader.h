#pragma once
#include <iiJson/Json.h>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace iiActorObject::NSCharacter::human_detail
{
using Value=ii::json::Value;
[[nodiscard]] const Value &field(const Value &value,std::string_view key);
template<class T> const T &as(const Value &value)
{
    const auto result=value.get<T>();
    if(!result)throw std::invalid_argument("Invalid template JSON field type");
    return result->get();
}
[[nodiscard]] double numeric(const Value &value);
[[nodiscard]] std::uint32_t pointId(const Value &value);
class Files final
{
  public:
    Files(std::filesystem::path directory,std::size_t limit);
    [[nodiscard]] std::string read(const std::filesystem::path &name);
    [[nodiscard]] Value json(const std::filesystem::path &name);
    [[nodiscard]] std::size_t remaining() const noexcept;
  private:
    std::filesystem::path root_;
    std::size_t left_;
};
} // namespace iiActorObject::NSCharacter::human_detail
