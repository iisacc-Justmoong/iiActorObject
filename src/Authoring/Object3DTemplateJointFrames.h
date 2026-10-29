#pragma once
#include "Object3DTemplate.h"

namespace iiActorObject::object3d_detail
{
// Internal variant for registration in a transformed object-space frame.
void bindJointFrames(Object3DData &data,std::span<const TemplateJointFrame> joints,Vector3 defaultHint,
                     Vector3 fallbackHint,double relativeParallelTolerance);
}
