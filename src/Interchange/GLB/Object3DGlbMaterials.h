#pragma once
#include "Object3DGlbJson.h"
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace iiActorObject
{
class GlbDocument;
struct ExternalMaterialIdentity;
struct Object3DData;
struct TopologyGlbTangentBinding;
}
namespace iiActorObject::object3d_detail::glb_import
{
// Material and image import for one GLB document. All references are borrowed
// from the import coordinator and must outlive this object.
class Materials final
{
public:
    Materials(const Value &root,const GlbDocument &document,const GlbImportOptions &options,
              Object3DData &data,std::vector<std::string> &warnings);
    void finish();
    std::size_t material(const Value *index);
    TopologyGlbTangentBinding tangentBinding(std::size_t index) const;
    void requirePrimitiveUvs(std::size_t index,std::size_t count) const;
    std::vector<ExternalMaterialIdentity> externalIdentities(bool &complete) const;
private:
    static std::string unique(std::string base,std::set<std::string> &names);
    std::pair<std::size_t,std::string> texture(const Value &info);
    std::size_t loadImage(std::size_t id);
    const Value &root_;
    const GlbDocument &document_;
    const GlbImportOptions &options_;
    Object3DData &data_;
    std::vector<std::string> &warnings_;
    std::vector<std::optional<std::size_t>> materialMap_,imageMap_;
    std::vector<bool> textureUsed_;
    std::optional<std::size_t> default_;
    std::set<std::string> materialNames_,imageNames_;
    std::size_t imageBytes_=0,resolverRequests_=0;
};
} // namespace iiActorObject::object3d_detail::glb_import
