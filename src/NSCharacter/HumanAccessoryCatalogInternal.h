#pragma once
#include "HumanAccessories.h"
#include "MakeHumanResourceReader.h"

namespace iiActorObject::NSCharacter::human_detail
{
struct AccessoryEntry final { HumanAccessoryInfo info;std::filesystem::path proxy;double roughness=0; };
[[nodiscard]] std::vector<AccessoryEntry> accessoryCatalog(Files &files);
[[nodiscard]] std::vector<AccessoryEntry> selectAccessories(const std::vector<AccessoryEntry> &entries,const HumanAccessorySelection &selection);
} // namespace iiActorObject::NSCharacter::human_detail
