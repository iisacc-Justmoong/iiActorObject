#pragma once
#include "Object3DGlbAnimationOptions.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace iiActorObject
{
struct Object3DData;
class Skeleton;
namespace object3d_detail::glb_export
{
struct Buffer;
struct MorphAnimationPlan final
{
    std::size_t mesh=0;
    std::vector<double> times;
    std::vector<std::size_t> curves;
    std::vector<std::uint8_t> values;
};
// Asset identity is supplied as a borrowed skeleton pointer and rig flag.
[[nodiscard]] std::size_t checkGlbAnimation(const Skeleton *skeleton,bool hasRig,const AnimationClip &clip,
    const GlbAnimationExportOptions &options,bool morphAnimation);
[[nodiscard]] std::vector<MorphAnimationPlan> planGlbMorphAnimation(const Object3DData &data,const AnimationClip &clip,
    const GlbAnimationExportOptions &options,const GlbMorphAnimationOptions &morphOptions,std::size_t usedKeys);
// Consumes plan value buffers while producing JSON and buffer views.
[[nodiscard]] std::string writeGlbAnimation(Buffer &buffer,const AnimationClip &clip,
    std::size_t boneNodeCount,std::vector<MorphAnimationPlan> &morphs);
}
}
