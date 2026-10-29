#pragma once
#include <iiActorObject/NSCharacter/AutoRig.h>
#include <iiActorObject/NSCharacter/CharacterAsset.h>

namespace iiActorObject::NSCharacter
{
struct BodyParameters final
{
    double height = 1.8;
    double legRatio = .52;
    double shoulderWidth = .40;
    double hipWidth = .32;
    double waistWidth = .28;
    double bodyDepth = .22;
    double armLength = .62;
    double headScale = 1;
    double noseScale = 1;
    double jawWidth = 1;
    double eyeSpacing = 1;
};
enum class GeneratedSkinBinding { Envelope, Surface };
struct GenerationOptions final
{
    // Samples along the canonical human's height, [32,160]. Same resolution -> same topology.
    unsigned resolution = 96;
    bool eyes = true;
    bool facialMorphs = true;
    GeneratedSkinBinding skinBinding = GeneratedSkinBinding::Envelope;
    // Used only for Surface; supplied seed spans must remain valid during generate().
    SurfaceBindOptions surfaceBinding = [] { SurfaceBindOptions value; value.weights.falloff=6; return value; }();
};
class IIACTOROBJECT_EXPORT HumanoidGenerator final
{
  public:
    // Independent implicit human surface, bilateral topology, semantic skeleton and skin weights.
    // Parameters deform canonical topology and joint positions together; no vendor asset is used.
    [[nodiscard]] static std::shared_ptr<const CharacterAsset> generate(
        Object3DEngine &engine, const BodyParameters &parameters = {},
        const GenerationOptions &options = {});
    [[nodiscard]] static std::shared_ptr<const CharacterAsset> generate(
        const Object3DReference &engine, const BodyParameters &parameters = {},
        const GenerationOptions &options = {});
};
} // namespace iiActorObject::NSCharacter
