#pragma once
#include "Object3DGlbTransform.h"
#include <iiActorObject/Skeleton.h>
#include <span>
#include <vector>

namespace iiActorObject::object3d_detail::glb_import
{
// Source nodes borrow the parsed JSON document; it must outlive the scene.
struct Node final {const Value *source=nullptr;std::size_t parent=NoBone;ufbx_matrix world=identity();bool active=false;};
struct Scene final
{
    std::vector<Node> nodes;
    std::vector<std::size_t> order;
    std::vector<std::size_t> meshNodes;
};
Scene readScene(const Value &root,const GlbImportOptions &options,std::vector<std::string> &warnings);
} // namespace iiActorObject::object3d_detail::glb_import
