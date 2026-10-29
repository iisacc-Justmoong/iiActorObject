#include <iiActorObject/Object3DGlbImportOptions.h>
#include <stdexcept>
#include <type_traits>

using namespace iiActorObject;
bool verifyGlbResolverHeader();
int main()
{
    static_assert(std::is_aggregate_v<GlbImportOptions>);
    GlbImportOptions options;
    if(options.maximumVertices!=2'000'000||options.maximumMorphDeltas!=16'000'000||
       options.imageResolver||options.allowDiscardAnimations||options.allowDiscardUnsupportedAttributes)
        throw std::runtime_error("Unexpected GLB import defaults");
    if(!verifyGlbResolverHeader())
        throw std::runtime_error("GLB image resolver contract failed");
}
