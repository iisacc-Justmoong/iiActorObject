#pragma once
#include "RigSampleCatalog.h"
#include <cstdint>

namespace iiActorObject::object3d_detail
{
[[nodiscard]] std::vector<std::uint8_t> encodeRigSampleCatalogs(
    std::span<const RigSampleCatalog> catalogs,std::size_t maximumBytes=64*1024*1024);
[[nodiscard]] std::vector<RigSampleCatalog> decodeRigSampleCatalogs(
    std::span<const std::uint8_t> bytes,std::size_t maximumBytes=64*1024*1024);
}
