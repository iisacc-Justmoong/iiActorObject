#pragma once
#include <iiActorObject/GlbDocument.h>
#include <cstddef>

namespace iiActorObject
{
class GlbImageResolver;
struct GlbImportOptions final
{
    GlbReadOptions document;
    std::size_t maximumNodes = 100000;
    std::size_t maximumMeshes = 4096;
    std::size_t maximumPrimitives = 16384;
    std::size_t maximumVertices = 2'000'000;
    std::size_t maximumTriangles = 2'000'000;
    std::size_t maximumBones = 4096;
    std::size_t maximumSkins = 4096;
    // Sum of skin.joints entries across the selected skins, including shared joints.
    std::size_t maximumSkinJointMappings = 1'000'000;
    std::size_t maximumMorphsPerMesh = 4096;
    std::size_t maximumMorphDeltas = 16'000'000;
    std::size_t maximumInfluencesPerVertex = 64;
    std::size_t maximumInfluenceSets = 256;
    std::size_t maximumUvSets = 32;
    std::size_t maximumImageBytes = 64 * 1024 * 1024;
    std::size_t maximumImageResolverRequests = 4096;
    double maximumJointMatrixError = 0.00001;
    // Loss must be explicitly requested; every discarded feature is reported.
    bool allowDiscardAnimations = false;
    bool allowDiscardUnsupportedAttributes = false;
    const GlbImageResolver *imageResolver = nullptr;
};
} // namespace iiActorObject
