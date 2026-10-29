#include "Object3DFbxBinary.h"
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail::fbx_binary
{
void integer(Bytes &b, std::uint64_t v, unsigned count)
{
    if (count > sizeof(v))
        throw std::invalid_argument("FBX integer width exceeds 64 bits");
    for (unsigned i = 0; i < count; ++i)
        b.push_back(static_cast<std::uint8_t>(v >> (i * 8)));
}
namespace
{
void patch32(Bytes &b, std::size_t offset, std::size_t value)
{
    if (value > std::numeric_limits<std::uint32_t>::max())
        throw std::overflow_error("FBX 7.4 file exceeds four GiB");
    for (unsigned i = 0; i < 4; ++i)
        b[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
} // namespace
Property prop(std::int32_t v)
{
    Property p{{'I'}};
    integer(p.bytes, static_cast<std::uint32_t>(v), 4);
    return p;
}
Property prop(std::int64_t v)
{
    Property p{{'L'}};
    integer(p.bytes, static_cast<std::uint64_t>(v), 8);
    return p;
}
Property prop(double v)
{
    if (!std::isfinite(v))
        throw std::invalid_argument("Nonfinite FBX number");
    Property p{{'D'}};
    integer(p.bytes, std::bit_cast<std::uint64_t>(v), 8);
    return p;
}
Property prop(std::string_view v)
{
    Property p{{'S'}};
    if (v.size() > 0xffffffffu)
        throw std::overflow_error("FBX string");
    integer(p.bytes, v.size(), 4);
    p.bytes.insert(p.bytes.end(), v.begin(), v.end());
    return p;
}
Property blob(std::span<const std::uint8_t> bytes)
{
    if(bytes.size()>0xffffffffu)throw std::length_error("FBX metadata blob limit");
    Property result{{'R'}};integer(result.bytes,bytes.size(),4);
    result.bytes.insert(result.bytes.end(),bytes.begin(),bytes.end());return result;
}
Property packed(std::span<const std::int32_t> values)
{
    Bytes bytes;for(const auto value:values)integer(bytes,static_cast<std::uint32_t>(value),4);return blob(bytes);
}
Property array(std::span<const double> values)
{
    if (values.size() > 0xffffffffu / 8)
        throw std::overflow_error("FBX array");
    Property p{{'d'}};
    integer(p.bytes, values.size(), 4);
    integer(p.bytes, 0, 4);
    integer(p.bytes, values.size() * 8, 4);
    for (double v : values)
    {
        if (!std::isfinite(v))
            throw std::invalid_argument("Nonfinite FBX array");
        integer(p.bytes, std::bit_cast<std::uint64_t>(v), 8);
    }
    return p;
}
Property array(std::span<const std::int32_t> values)
{
    if (values.size() > 0xffffffffu / 4)
        throw std::overflow_error("FBX array");
    Property p{{'i'}};
    integer(p.bytes, values.size(), 4);
    integer(p.bytes, 0, 4);
    integer(p.bytes, values.size() * 4, 4);
    for (auto v : values)
        integer(p.bytes, static_cast<std::uint32_t>(v), 4);
    return p;
}
Property array(std::span<const std::int64_t> values)
{
    if(values.size()>0xffffffffu/8)throw std::overflow_error("FBX array");
    Property p{{'l'}}; integer(p.bytes,values.size(),4);integer(p.bytes,0,4);integer(p.bytes,values.size()*8,4);
    for(const auto v:values)integer(p.bytes,static_cast<std::uint64_t>(v),8);
    return p;
}
Property array(std::span<const float> values)
{
    if(values.size()>0xffffffffu/4)throw std::overflow_error("FBX array");
    Property p{{'f'}}; integer(p.bytes,values.size(),4);integer(p.bytes,0,4);integer(p.bytes,values.size()*4,4);
    for(const auto v:values)
    {
        if(!std::isfinite(v))throw std::invalid_argument("Nonfinite FBX array");
        integer(p.bytes,std::bit_cast<std::uint32_t>(v),4);
    }
    return p;
}
void encode(Bytes &out, const Node &n)
{
    if (n.name.size() > 255 || n.properties.size() > 0xffffffffu)
        throw std::overflow_error("FBX node header");
    const auto start = out.size();
    out.resize(start + 13, 0);
    out[start + 12] = static_cast<std::uint8_t>(n.name.size());
    out.insert(out.end(), n.name.begin(), n.name.end());
    const auto beginProperties = out.size();
    for (const auto &p : n.properties)
        out.insert(out.end(), p.bytes.begin(), p.bytes.end());
    patch32(out, start + 4, n.properties.size());
    patch32(out, start + 8, out.size() - beginProperties);
    for (const auto &c : n.children)
        encode(out, c);
    if (!n.children.empty())
        out.resize(out.size() + 13, 0);
    patch32(out, start, out.size());
}
Node value(std::string n, Property p)
{
    return {std::move(n), {std::move(p)}, {}};
}
} // namespace iiActorObject::object3d_detail::fbx_binary
