#include <iiActorObject/GlbImageResolver.h>

using namespace iiActorObject;
namespace
{
class Resolver final : public GlbImageResolver
{
public:
    std::optional<GlbResolvedImage> resolve(const GlbImageRequest &request) const override
    {return GlbResolvedImage{{1,2,3},std::string(request.uri)};}
};
}
bool verifyGlbResolverHeader()
{
    Resolver resolver;
    const auto result=resolver.resolve({"name","asset.png","image/png",{},32});
    return result&&result->pngBytes==std::vector<std::uint8_t>({1,2,3})&&result->notice=="asset.png";
}
