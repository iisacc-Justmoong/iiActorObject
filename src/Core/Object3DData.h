#pragma once
#include <iiActorObject/Object3DMesh.h>
#include <iiActorObject/Object3DImage.h>
#include <iiActorObject/Object3DMaterial.h>
#include <iiActorObject/PoseReference.h>
#include <iiActorObject/RigSampleCatalogCore.h>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace iiActorObject
{
enum class Object3DKind
{
    Automatic,
    StaticMesh,
    SkeletalMesh
};
struct Object3DJointSemantic final
{
    std::string name;
    BoneIndex bone = NoBone;
};
struct Object3DData final
{
    std::string name = "Object3D";
    Object3DKind kind = Object3DKind::Automatic;
    // Empty skeletons describe static objects; the engine supplies a rigid root binding.
    std::vector<Bone> bones;
    std::vector<Object3DJointSemantic> semantics;
    std::vector<Object3DMaterial> materials{{}};
    std::vector<Object3DMesh> meshes;
    std::vector<Object3DImage> images;
    // Original geometry/bind remains canonical; this is a recomputable authoring pose.
    std::optional<PoseReference> referencePose;
    // Sample provenance is core-owned; it does not own or replace the source rig.
    std::vector<RigSampleCatalog> rigSampleCatalogs;
};

} // namespace iiActorObject
