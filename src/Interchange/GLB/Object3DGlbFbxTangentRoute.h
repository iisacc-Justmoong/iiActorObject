#pragma once
#include "Object3DFbxTangentProjection.h"
#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace iiActorObject
{
struct Object3DMesh;
struct TopologyTangentSourceLayer;
namespace object3d_detail::glb_export
{
struct GlbFbxTangentRoute final
{
    const TopologyTangentSourceLayer *layer=nullptr;
    const FbxTangentProjection *projection=nullptr;
    std::vector<std::string_view> notes;
    std::optional<std::size_t> unboundUv;
};
// Borrows an immutable mesh. Returned pointers remain valid only while the
// selector and the unchanged mesh live. The projection is calculated once.
class GlbFbxTangentSelector final
{
  public:
    explicit GlbFbxTangentSelector(const Object3DMesh &mesh);
    GlbFbxTangentSelector(Object3DMesh &&)=delete;
    GlbFbxTangentSelector(const Object3DMesh &&)=delete;
    GlbFbxTangentSelector(const GlbFbxTangentSelector &)=delete;
    GlbFbxTangentSelector &operator=(const GlbFbxTangentSelector &)=delete;
    // An absent index means the material has no normal-map binding.
    [[nodiscard]] GlbFbxTangentRoute select(std::optional<std::size_t> normalUv);
  private:
    const Object3DMesh *mesh_;
    bool hasSource_=false,valid_=false,projectionAttempted_=false;
    std::optional<FbxTangentProjection> projection_;
    std::string_view projectionFailure_;
};
}
} // namespace iiActorObject
