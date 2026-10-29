#pragma once
#include <iiActorObject/Animation.h>
#include <iiActorObject/Object3DEngine.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace iiActorObject
{
struct Object3DProjectAnimation final
{
    std::string name;
    Object3DHandle object; // Borrowed snapshot against which the indices are defined.
    std::shared_ptr<const AnimationClip> clip;
    std::vector<AnimationMorphBinding> morphBindings;
    // Original timeline origin when known; clip times remain relative to zero.
    std::optional<double> sourceStartSeconds;
};
} // namespace iiActorObject
