#pragma once
#include <iiActorObject/Object3DInterchange.h>

namespace iiActorObject
{
// Implementations choose where untrusted FBX image names may resolve.
class FbxExternalImageSource
{
  public:
    virtual ~FbxExternalImageSource()=default;
    [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> read(
        const FbxImageRequest &request,std::size_t maximumBytes) const=0;
};
struct FbxVisualImageLimits final
{
    std::size_t maxSourceBytes=64*1024*1024;
    std::size_t maxDecodedBytes=256*1024*1024;
    std::uint32_t maxDimension=16384;
};
// Optional iiVisualCodec adapter. The source is borrowed through the synchronous import.
class FbxVisualImageResolver final:public FbxImageResolver
{
  public:
    explicit FbxVisualImageResolver(const FbxExternalImageSource &source,FbxVisualImageLimits limits={});
    [[nodiscard]] std::optional<FbxResolvedImage> resolve(const FbxImageRequest &request) const override;
  private:
    const FbxExternalImageSource *source_;
    FbxVisualImageLimits limits_;
};
} // namespace iiActorObject
