#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace iiActorObject
{
struct MorphTarget;
struct Vector3;
namespace object3d_detail::glb_export
{
// Low-level JSON and little-endian primitives shared by GLB export stages.
[[nodiscard]] std::string quote(std::string_view text);
[[nodiscard]] std::ostringstream stream();
void u32(std::vector<std::uint8_t> &out,std::uint32_t value);
void u16(std::vector<std::uint8_t> &out,std::uint16_t value);
[[nodiscard]] float narrow(double value);
void f32(std::vector<std::uint8_t> &out,double value);
void align(std::vector<std::uint8_t> &out);
void jsonArray(std::ostream &out,std::span<const std::string> values);

// Owns serialized buffer views and accessor declarations, but no scene or asset.
struct Buffer final
{
    std::vector<std::uint8_t> bytes;
    std::vector<std::string> views,accessors;
    std::size_t view(std::vector<std::uint8_t> data,unsigned target=0);
    std::size_t accessor(std::size_t viewIndex,unsigned component,std::size_t count,
                         std::string_view type,std::string extra={});
    std::size_t vectors(std::span<const Vector3> values,bool bounds=false);
    std::size_t sparse(const MorphTarget &morph,std::size_t count,bool normal);
};
}
}
