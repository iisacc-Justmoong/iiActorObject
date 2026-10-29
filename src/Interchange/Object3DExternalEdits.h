#pragma once
#include <iiActorObject/Object3DAsset.h>
#include <iiActorObject/Object3DExternalIdentity.h>
#include <cstdint>

namespace iiActorObject
{
struct Object3DExternalSnapshot final
{
    const Object3DAsset &object;
    const Object3DExternalIdentity &identity;
};
struct Object3DExternalOptions final
{
    std::string document;
    double positionTolerance=1e-6,attributeTolerance=1e-6,matrixTolerance=1e-6;
    double referenceTolerance=2e-5;
    bool overwriteConflicts=false;
    std::optional<std::uint64_t> replaceLayer;
};
struct Object3DExternalReport final
{
    std::size_t positions=0,weightRows=0,jointTransforms=0,parents=0,names=0,morphs=0,uvs=0,materials=0;
    std::optional<std::uint64_t> layer;
    double maximumJointMatrixError=0;
    bool applied=false;
    std::size_t normals=0;
    [[nodiscard]] bool hasEdits()const noexcept{return positions||weightRows||jointTransforms||parents||names||morphs||uvs||materials||normals;}
    [[nodiscard]] bool changed()const noexcept{return applied;}
};
// DTO identities refer to immutable core-owned snapshots. They do not own an engine.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DExternalIdentity makeObject3DExternalIdentity(
    const Object3DData &data,std::string document);
// Apply only baseline-to-edited differences to current canonical source IDs.
// Full source point/bone coverage is required; unchanged app coordinate frames cancel out.
[[nodiscard]] IIACTOROBJECT_EXPORT Object3DExternalReport applyObject3DExternalEdits(
    Object3DData &current,const Object3DAsset &source,Object3DExternalSnapshot baseline,
    Object3DExternalSnapshot edited,const Object3DExternalOptions &options);
} // namespace iiActorObject
