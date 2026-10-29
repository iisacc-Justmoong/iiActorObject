#pragma once
#include "Object3DGlbJson.h"
#include "Object3DGlbGeometryBudget.h"
#include <set>
#include <string>
#include <vector>

namespace iiActorObject { class GlbDocument; struct Object3DMesh; }
namespace iiActorObject::object3d_detail::glb_import
{
struct Node;
struct Skin;
class Materials;
// Builds a mesh snapshot using borrowed document, scene, skin, and material data.
Object3DMesh readMesh(const Value &root,const GlbDocument &document,const Node &node,const Skin &skin,
                      bool quantized,const GlbImportOptions &options,GeometryBudgets &budget,
                      Materials &materials,std::set<std::string> &meshNames,std::vector<std::string> &warnings,
                      bool externalProvenance,bool &restoredPointIdentity);
} // namespace iiActorObject::object3d_detail::glb_import
