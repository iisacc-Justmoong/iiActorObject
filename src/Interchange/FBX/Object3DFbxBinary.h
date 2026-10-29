#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace iiActorObject::object3d_detail::fbx_binary
{
using Bytes = std::vector<std::uint8_t>;
struct Property final { Bytes bytes; };
struct Node final
{
    std::string name;
    std::vector<Property> properties;
    std::vector<Node> children;
};
// FBX 7.4 uses little-endian values and absolute 32-bit node offsets.
void integer(Bytes &bytes,std::uint64_t value,unsigned count);
Property prop(std::int32_t value);
Property prop(std::int64_t value);
Property prop(double value);
Property prop(std::string_view value);
Property blob(std::span<const std::uint8_t> bytes);
Property packed(std::span<const std::int32_t> values);
Property array(std::span<const double> values);
Property array(std::span<const std::int32_t> values);
Property array(std::span<const std::int64_t> values);
Property array(std::span<const float> values);
void encode(Bytes &out,const Node &node);
Node value(std::string name,Property property);
} // namespace iiActorObject::object3d_detail::fbx_binary
