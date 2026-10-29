#pragma once
#include <iiActorObject/Export.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace iiActorObject
{
struct GlbImageRequest final
{
    // Views are borrowed for resolve() only. An empty URI denotes embedded data.
    std::string_view name, uri, mimeType;
    std::span<const std::uint8_t> embeddedBytes;
    std::size_t remainingBytes = 0;
};
struct GlbResolvedImage final
{
    // The core stores PNG; the host may resolve a URI or convert another format.
    std::vector<std::uint8_t> pngBytes;
    std::string notice;
};
class IIACTOROBJECT_EXPORT GlbImageResolver
{
  public:
    virtual ~GlbImageResolver() = default;
    [[nodiscard]] virtual std::optional<GlbResolvedImage> resolve(const GlbImageRequest &request) const = 0;
};
} // namespace iiActorObject
