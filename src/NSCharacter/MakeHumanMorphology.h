#pragma once
#include "MakeHumanResourceReader.h"
#include <iiActorObject/Object3DTemplate.h>

namespace iiActorObject::NSCharacter::human_detail
{
void semanticControls(Object3DTemplateDefinition &definition,const Value &modifiers);
void macroAssets(Object3DData &data,Object3DTemplateDefinition &definition,Files &files,double scale);
} // namespace iiActorObject::NSCharacter::human_detail
