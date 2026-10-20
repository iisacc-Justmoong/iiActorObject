#pragma once

#include <iiActorObject/Actor.h>

#include <array>
#include <span>
#include <string>
#include <vector>

namespace iiActorObject {

enum class UsdUpAxis { Y, Z };

struct UsdStageOptions {
    std::string rootPrimName = "World";
    UsdUpAxis upAxis = UsdUpAxis::Z;
    double metersPerUnit = 1.0;
};

struct UsdAssetReference {
    // nullptr targets the Actor's root. Referenced content gets its own child prim.
    const SceneComponent* component = nullptr;
    std::string assetPath;
    // Empty selects the referenced layer's defaultPrim.
    std::string assetPrimPath;
    bool instanceable = false;
};

struct UsdActorPlacement {
    const Actor* actor = nullptr;
    // Empty generates Actor_<id>. Explicit names use the ASCII USD identifier subset.
    std::string primName;
    std::vector<UsdAssetReference> references;
};

struct UsdComponentBinding {
    // Index in the exported Actor::components() snapshot, not a persistent ID.
    std::size_t componentIndex = 0;
    std::string primPath;
};

struct UsdAssetBinding {
    std::size_t referenceIndex = 0;
    std::string primPath;
};

struct UsdActorBinding {
    ActorId actorId;
    std::string primPath;
    std::vector<UsdComponentBinding> components;
    std::vector<UsdAssetBinding> assets;
};

struct UsdLayer {
    std::string usda;
    std::vector<UsdActorBinding> actors;
};

using UsdMatrix4 = std::array<std::array<double, 4>, 4>;

// USD row-vector matrix = transpose of the iiActorObject column-vector matrix.
[[nodiscard]] IIACTOROBJECT_EXPORT UsdMatrix4 toUsdMatrix(const Matrix4& matrix);

// Synchronous snapshot. Does not retain Actor pointers, run hooks or perform file I/O.
// Invalid paths, duplicate placements, destroyed Actors and non-finite values throw
// std::invalid_argument. Actor/component data must not change during the call.
[[nodiscard]] IIACTOROBJECT_EXPORT UsdLayer exportUsdLayer(
    std::span<const UsdActorPlacement> placements, const UsdStageOptions& options = {});

} // namespace iiActorObject
