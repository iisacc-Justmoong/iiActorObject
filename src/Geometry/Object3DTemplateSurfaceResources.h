#pragma once
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace iiActorObject
{
struct Object3DData;
struct Object3DMaterial;
struct Object3DMesh;
class Object3DImage;
namespace object3d_detail
{
[[nodiscard]] bool sameTemplateMaterial(const Object3DMaterial &a,const Object3DMaterial &b);
[[nodiscard]] bool templateMeshUsesMaterial(const Object3DMesh &mesh,std::size_t material);
[[nodiscard]] bool templateMaterialUsesImage(const Object3DMaterial &material,std::size_t image);
void requireSingleTemplateMaterial(const Object3DMesh &mesh);

// Each non-null pointer borrows one staged resource array from an editing layer.
// The arrays and both Object3DData values must outlive the mapper.
struct TemplateSurfaceResourceLayer final
{
    std::vector<Object3DImage> *images=nullptr;
    std::vector<Object3DMaterial> *materials=nullptr;
};
// Updates staged base/layer resources; the caller owns publication and rollback.
// On failure, discard the staging arrays rather than relying on partial rollback.
class TemplateSurfaceResources final
{
  public:
    TemplateSurfaceResources(Object3DData &base,const Object3DData &candidate,
        std::span<const TemplateSurfaceResourceLayer> layers);
    void linkMaterial(std::size_t candidate,std::size_t existing);
    void linkRetainedImages();
    [[nodiscard]] std::size_t material(std::size_t candidate);
  private:
    static std::string unique(std::set<std::string> &names,std::string_view prefix);
    static Object3DImage copyImage(std::string name,const Object3DImage &image);
    void linkImage(std::optional<std::size_t> incoming,std::optional<std::size_t> existing);
    std::size_t image(std::size_t index);
    Object3DData &base_;
    const Object3DData &candidate_;
    std::vector<TemplateSurfaceResourceLayer> layers_;
    std::set<std::string> materialNames_,imageNames_;
    std::map<std::size_t,std::size_t> materials_,images_;
    std::set<std::size_t> ambiguousImages_;
};
}
}
