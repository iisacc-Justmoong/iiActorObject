#include <iiActorObject/FbxVisualImageResolver.h>
#include <optional>
#include <vector>
class EmptySource final : public iiActorObject::FbxExternalImageSource {
public:
    std::optional<std::vector<std::uint8_t>> read(const iiActorObject::FbxImageRequest&,std::size_t) const override {
        return std::nullopt;
    }
};
int main() {
    EmptySource source;
    iiActorObject::FbxVisualImageResolver resolver(source);
    const iiActorObject::FbxImageRequest request{};
    return resolver.resolve(request).has_value() ? 1 : 0;
}
