#pragma once
#include <cstdint>
#include <string>

namespace iiActorObject
{
using Object3DLayerId=std::uint64_t;
enum class Object3DLayerEvaluation : std::uint8_t
{
    Deferred,
    EvaluatedSurface
};
struct Object3DEditLayerInfo final
{
    Object3DLayerId id=0;
    std::string label;
    bool enabled=true;
    double weight=1;
    Object3DLayerEvaluation evaluation=Object3DLayerEvaluation::Deferred;
};
} // namespace iiActorObject
