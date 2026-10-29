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
// Path fields are untrusted FBX data, not paths opened by iiActorObject.
// Views expire when resolve() returns. Embedded bytes are supplied for non-PNG
// content; an empty span denotes a reference to be resolved by the host.
// The resolver is borrowed for this import only.
struct FbxImageRequest final
{
    std::string_view textureName,filename,absoluteFilename,relativeFilename;
    std::size_t remainingBytes=0;
    std::span<const std::uint8_t> embeddedBytes;
};
struct FbxResolvedImage final
{
    std::vector<std::uint8_t> bytes;
    std::string mimeType="image/png";
    // Optional bounded diagnostic when the resolver changed the original resource.
    std::string notice;
};
class IIACTOROBJECT_EXPORT FbxImageResolver
{
  public:
    virtual ~FbxImageResolver()=default;
    [[nodiscard]] virtual std::optional<FbxResolvedImage> resolve(const FbxImageRequest &request) const=0;
};
} // namespace iiActorObject
