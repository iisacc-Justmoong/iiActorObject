#include "MeshPointMaskValidation.h"
#include "MeshPointWeightValidation.h"
#include "Object3DData.h"
#include <set>
#include <stdexcept>

namespace iiActorObject
{
void validateMeshPointMasks(const Object3DData &data,std::span<const MeshPointMask> masks)
{
    std::set<std::size_t> seen;
    for(const auto &mask:masks)
    {
        const auto &mesh=data.meshes.at(mask.mesh);
        if(!seen.insert(mask.mesh).second)throw std::invalid_argument("Duplicate mesh point mask");
        object3d_detail::validatePointWeights(mask.weights,
            mesh.topology?mesh.topology->points.size():mesh.geometry.vertices.size());
    }
}
} // namespace iiActorObject
