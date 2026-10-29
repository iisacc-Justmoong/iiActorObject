#pragma once
#include <iiActorObject/Export.h>
#include <iiActorObject/MeshPointMask.h>
#include <span>

namespace iiActorObject
{
struct Object3DData;
// Validates mesh indices, unique rows, dimensions, and finite unit-interval weights.
IIACTOROBJECT_EXPORT void validateMeshPointMasks(const Object3DData &data,std::span<const MeshPointMask> masks);
} // namespace iiActorObject
