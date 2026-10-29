#pragma once
#include <iiActorObject/MeshData.h>

namespace iiActorObject
{
IIACTOROBJECT_EXPORT void validateMeshGeometry(const SkeletalMeshLod &mesh);
// Area weighted normals and morph normal deltas. Strong exception guarantee.
IIACTOROBJECT_EXPORT void recomputeNormals(SkeletalMeshLod &mesh);
} // namespace iiActorObject
