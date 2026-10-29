#pragma once
#include "Object3DTemplateRigTransfer.h"
#include "Object3DTemplateRigProjection.h"

namespace iiActorObject::object3d_detail
{
struct FittedTemplateShape final
{
    Object3DData data;
    TemplateRigShapeFitReport report;
    std::optional<RegisteredTemplateReference::YawEstimate> yaw;
};
[[nodiscard]] FittedTemplateShape fitTemplateShape(const Object3DTemplate &program,const Object3DData &target,
    std::span<const TemplateRigMeshPair> pairs,std::span<const TemplateValue> initialValues,
    const TemplateRigProjectionOptions &projection,const TemplateRigShapeFitOptions &options);
} // namespace iiActorObject::object3d_detail
