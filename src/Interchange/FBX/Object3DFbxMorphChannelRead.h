#pragma once
#include "Object3DFbxAnimationData.h"
#include "Object3DProgressiveMorphData.h"
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace iiActorObject
{
struct MeshTopology;
struct Object3DMesh;
namespace object3d_detail
{
struct FbxMorphChannelOptions final
{
    std::size_t maximumMorphs;
    bool restoreMetadata,importAnimation;
    // Borrowed, initially empty output program; nullptr rejects progressive channels.
    ProgressiveMorphProgram *program;
};
// Accumulates source channel identity across meshes without publishing an asset.
class FbxMorphChannelReader final
{
  public:
    explicit FbxMorphChannelReader(FbxMorphChannelOptions options);
    [[nodiscard]] std::vector<std::string> readMesh(const ufbx_mesh &source,Object3DMesh &mesh,
        MeshTopology &topology,std::size_t meshIndex,const ufbx_matrix &geometry,
        bool hasAuthoringMetadata,ufbx_mirror_axis mirrorAxis);
    void finish(std::span<const Object3DMesh> meshes);
    [[nodiscard]] const FbxImportedMorphs &animatedMorphs() const noexcept {return animatedMorphs_;}
    [[nodiscard]] const std::vector<FbxImportedProgressiveControl> &animatedProgressive() const noexcept {return animatedProgressive_;}
  private:
    struct Group final {std::size_t program,expected,seen;bool sparse;};
    FbxMorphChannelOptions options_;
    std::set<std::string> progressiveControls_;
    std::map<std::string,std::set<std::string>,std::less<>> progressiveTargets_;
    std::map<std::uint32_t,std::size_t> progressiveChannels_;
    std::map<std::uint32_t,Group> progressiveGroups_;
    std::set<std::size_t> sparsePrograms_;
    std::size_t progressiveKeyCount_=0,progressiveMeshRefs_=0,progressiveKeyInstances_=0;
    FbxImportedMorphs animatedMorphs_;
    std::vector<FbxImportedProgressiveControl> animatedProgressive_;
};
}
}
