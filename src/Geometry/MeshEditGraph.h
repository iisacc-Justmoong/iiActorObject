#pragma once
// Private, transient geometry view shared by selection and painting kernels.
#include "MeshEditing.h"
#include "MeshPointWeightValidation.h"
#include "MeshSurface.h"
namespace iiActorObject::object3d_detail
{
struct MeshEditGraph final
{
    std::vector<Vector3> positions;
    std::vector<bool> visible;
    std::vector<std::vector<std::uint32_t>> neighbours,polygons;
    std::vector<std::uint32_t> triangles,renderToPoint,triangleFaces;
    std::unique_ptr<MeshSurface> surface;
    explicit MeshEditGraph(const Object3DMesh &);
    void validate(const MeshBrushSettings &,std::size_t samples) const;
    [[nodiscard]] std::vector<double> footprint(MeshBrushSample,const MeshBrushSettings &);
};
void refreshEditedMesh(Object3DMesh &,bool positionsChanged);
}
