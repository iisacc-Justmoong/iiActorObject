#pragma once
#include "HumanAccessoryCatalogInternal.h"
#include <iiActorObject/Object3DTemplate.h>
#include <span>

namespace iiActorObject::NSCharacter::human_detail
{
void faceAssets(Object3DData &data,Object3DTemplateDefinition &definition,Files &files,double scale);
void accessoryAssets(Object3DData &data,Object3DTemplateDefinition &definition,Files &files,double scale,
    std::span<const AccessoryEntry> entries,bool hideCoveredBody);
} // namespace iiActorObject::NSCharacter::human_detail
