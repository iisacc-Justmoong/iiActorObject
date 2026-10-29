#pragma once
#include "HumanSkin.h"
namespace iiActorObject::NSCharacter::human_skin_detail
{
struct Prepared final
{
    std::string id;
    std::vector<std::uint8_t> bytes;
    double roughness=0;
    std::size_t bytesRead=0;
};
[[nodiscard]] Prepared prepare(const HumanSkinSelection &selection,std::size_t remaining);
[[nodiscard]] bool applyPrepared(Object3DData &data,std::size_t mesh,Prepared skin);
// Uses a shared caller budget; data is only published by the core after success.
[[nodiscard]] bool apply(Object3DData &data,const HumanSkinSelection &selection,std::size_t &remaining);
}
