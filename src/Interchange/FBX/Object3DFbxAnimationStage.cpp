#include "Object3DFbxAnimationStage.h"
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail
{
void validateFbxAnimationClips(std::span<const Bone> bones,std::span<const FbxImportedClip> clips)
{
    std::vector<Bone> validationBones(bones.begin(),bones.end());
    if(validationBones.empty())validationBones.push_back({"root",NoBone,{}});
    auto skeleton=std::make_shared<Skeleton>(std::move(validationBones));
    for(const auto &clip:clips)(void)AnimationClip(skeleton,clip.duration,clip.tracks,clip.curves);
}
std::vector<FbxStagedAnimation> stageFbxAnimationClips(std::shared_ptr<const Skeleton> skeleton,
    std::span<const Object3DMesh> meshes,std::vector<FbxImportedClip> clips)
{
    std::vector<FbxStagedAnimation> built;built.reserve(clips.size());
    for(auto &clip:clips)
    {
        for(const auto &binding:clip.morphBindings)
            if(binding.curve>=clip.curves.size()||binding.mesh>=meshes.size()||
               binding.target>=meshes[binding.mesh].geometry.morphTargets.size())
                throw std::logic_error("Imported FBX morph mapping is invalid");
        built.push_back({std::move(clip.name),clip.sourceStartSeconds,
            std::make_shared<AnimationClip>(skeleton,clip.duration,std::move(clip.tracks),std::move(clip.curves)),
            std::move(clip.morphBindings)});
    }
    return built;
}
}
