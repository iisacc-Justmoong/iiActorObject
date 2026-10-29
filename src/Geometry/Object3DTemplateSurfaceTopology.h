#pragma once
#include <cstdint>
#include <vector>

namespace iiActorObject
{
struct MeshTopology;
struct Object3DMesh;
struct SurfaceBinding;
namespace object3d_detail
{
// Compares stable source IDs and surviving triangles while permitting visibility changes.
void validateTemplateVisibilityLayout(const MeshTopology &old,const MeshTopology &next,bool ignoreMaterials=false);
void validateRetainedTemplateLayout(const Object3DMesh &old,const Object3DMesh &next);
// These operations leave their output unchanged if validation or allocation fails.
void preserveTemplateGroundingSupport(std::vector<std::uint32_t> &supportPoints,const MeshTopology &old,const MeshTopology &next);
void remapTemplateVisibilityBinding(SurfaceBinding &binding,const MeshTopology &old,const MeshTopology &next);
}
}
