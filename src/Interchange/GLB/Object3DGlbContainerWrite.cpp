#include "Object3DGlbContainerWrite.h"
#include "Object3DGlbBuffer.h"
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace iiActorObject::object3d_detail::glb_export
{
std::vector<std::uint8_t> writeGlbContainer(std::string_view json,std::span<const std::uint8_t> binary)
{
    constexpr auto maximum=std::numeric_limits<std::uint32_t>::max();
    if(json.size()>maximum||binary.size()>maximum)throw std::overflow_error("GLB exceeds four GiB");
    const std::uint64_t jsonBytes=std::uint64_t(json.size())+(4-json.size()%4)%4;
    const std::uint64_t binaryBytes=std::uint64_t(binary.size())+(4-binary.size()%4)%4;
    const std::uint64_t total=12+8+jsonBytes+8+binaryBytes;
    if(total>maximum)throw std::overflow_error("GLB exceeds four GiB");
    std::vector<std::uint8_t> out;out.reserve(static_cast<std::size_t>(total));
    u32(out,0x46546c67);u32(out,2);u32(out,static_cast<std::uint32_t>(total));
    u32(out,static_cast<std::uint32_t>(jsonBytes));u32(out,0x4e4f534a);
    out.insert(out.end(),json.begin(),json.end());
    out.insert(out.end(),static_cast<std::size_t>(jsonBytes-json.size()),' ');
    u32(out,static_cast<std::uint32_t>(binaryBytes));u32(out,0x004e4942);
    out.insert(out.end(),binary.begin(),binary.end());
    out.insert(out.end(),static_cast<std::size_t>(binaryBytes-binary.size()),0);
    return out;
}
} // namespace iiActorObject::object3d_detail::glb_export
