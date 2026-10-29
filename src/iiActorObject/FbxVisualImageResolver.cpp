#include "FbxVisualImageResolver.h"
#include <iiVisualCodec/ImageCodec.h>
#include <algorithm>
#include <array>
#include <stdexcept>

namespace iiActorObject
{
FbxVisualImageResolver::FbxVisualImageResolver(const FbxExternalImageSource &source,FbxVisualImageLimits limits)
    :source_(&source),limits_(limits)
{
    if(!limits.maxSourceBytes||limits.maxSourceBytes>256*1024*1024||!limits.maxDecodedBytes||
       limits.maxDecodedBytes>512*1024*1024||!limits.maxDimension||limits.maxDimension>16384)
        throw std::invalid_argument("Invalid FBX image codec limits");
}
std::optional<FbxResolvedImage> FbxVisualImageResolver::resolve(const FbxImageRequest &request) const
{
    std::optional<std::vector<std::uint8_t>> owned;
    auto bytes=request.embeddedBytes;
    if(bytes.empty())
    {
        owned=source_->read(request,limits_.maxSourceBytes);if(!owned)return std::nullopt;
        bytes=*owned;
    }
    if(bytes.size()>limits_.maxSourceBytes)throw std::length_error("Source image byte budget");
    const auto outputLimit=std::min<std::size_t>(request.remainingBytes,64*1024*1024);
    ii::visual::image::Limits codecLimits;
    codecLimits.input_bytes=limits_.maxSourceBytes;codecLimits.output_bytes=outputLimit;
    codecLimits.pixel_bytes=limits_.maxDecodedBytes;codecLimits.dimension=limits_.maxDimension;
    constexpr std::array<std::uint8_t,8> png{137,80,78,71,13,10,26,10};
    if(bytes.size()>=png.size()&&std::equal(png.begin(),png.end(),bytes.begin()))
    {
        if(bytes.size()>outputLimit)throw std::length_error("PNG byte budget");
        const auto info=ii::visual::image::inspect(bytes,codecLimits);
        if(!info)throw std::invalid_argument("External PNG inspection failed: "+info.error().message);
        if(owned)return FbxResolvedImage{std::move(*owned),"image/png",{}};
        return FbxResolvedImage{std::vector<std::uint8_t>(bytes.begin(),bytes.end()),"image/png",{}};
    }
    auto decoded=ii::visual::image::decode(bytes,codecLimits);
    if(!decoded)throw std::invalid_argument("External image decode failed: "+decoded.error().message);
    auto encoded=ii::visual::image::encode_png(decoded->view(),codecLimits);
    if(!encoded)throw std::invalid_argument("External image PNG encoding failed: "+encoded.error().message);
    return FbxResolvedImage{std::move(*encoded),"image/png",
        "An FBX image was transcoded to PNG; color profile and orientation follow the host codec policy."};
}
} // namespace iiActorObject
