#pragma once
#include "Object3DGlbJson.h"
#include <iiActorObject/BoneIndex.h>
#include <optional>
#include <string>
#include <vector>

namespace iiActorObject::object3d_detail::glb_import
{
// Parsed identifiers for one primitive; buffer decoding remains the mesh reader's responsibility.
struct AttributeSet final
{
    std::size_t position=NoBone;
    std::optional<std::size_t> normal,tangent,pointIdentity;
    std::vector<std::size_t> uv,joints,weights;
};
struct Target final {std::optional<std::size_t> position,normal,tangent;};
AttributeSet attributes(const Value &value,const GlbImportOptions &options,std::vector<std::string> &warnings);
std::vector<Target> targets(const Value &primitive,const GlbImportOptions &options,std::vector<std::string> &warnings);
std::string bundleKey(const AttributeSet &attributes,const std::vector<Target> &targets);
std::string pointKey(const AttributeSet &attributes,const std::vector<Target> &targets);
std::vector<std::string> morphNames(const Value &mesh,std::size_t count,std::vector<std::string> &warnings);
std::vector<double> initialWeights(const Value &mesh,const Value &node,std::size_t count);
} // namespace iiActorObject::object3d_detail::glb_import
