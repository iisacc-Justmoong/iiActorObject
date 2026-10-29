#pragma once
#include "Object3DGlbScene.h"
#include <optional>
#include <vector>

namespace iiActorObject { class GlbDocument; struct Object3DData; }
namespace iiActorObject::object3d_detail::glb_import
{
struct Skin final {
    bool present=false;
    std::size_t sourceIndex=NoBone;
    std::vector<BoneIndex> sourceToBone;
    ufbx_matrix bind=identity();
    double maximumError=0;
    BoneIndex rigidBone=NoBone;
};
struct SkinSet final {
    std::vector<Skin> skins;
    std::vector<std::size_t> nodeSlots;
    std::vector<BoneIndex> nodeRigidBones;
    Skin unskinned;
    double maximumError=0;
    std::optional<BoneIndex> syntheticRoot;
};
// Builds the source-to-core joint mappings without owning the source document.
SkinSet readSkins(const Value &root,const GlbDocument &document,const Scene &scene,
                  const GlbImportOptions &options,Object3DData &data,std::vector<std::string> &warnings);
void readSemantics(const Scene &scene,const Skin &skin,bool ambiguous,const GlbImportOptions &options,
                   Object3DData &data,std::vector<std::string> &warnings);
} // namespace iiActorObject::object3d_detail::glb_import
