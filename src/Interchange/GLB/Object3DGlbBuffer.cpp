#include "Object3DGlbBuffer.h"
#include "MeshData.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail::glb_export
{
std::string quote(std::string_view text)
{
    constexpr char hex[] = "0123456789abcdef";
    std::string out = "\"";
    for (const unsigned char c : text)
    {
        if (c == '"' || c == '\\')
        {
            out += '\\';
            out += static_cast<char>(c);
        }
        else if (c < 32)
        {
            out += "\\u00";
            out += hex[c >> 4];
            out += hex[c & 15];
        }
        else
            out += static_cast<char>(c);
    }
    out += '"';
    return out;
}
std::ostringstream stream()
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17);
    return out;
}
void u32(std::vector<std::uint8_t> &out, std::uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        out.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}
void u16(std::vector<std::uint8_t> &out, std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
}
float narrow(double v)
{
    if (!std::isfinite(v) || std::abs(v) > std::numeric_limits<float>::max())
        throw std::overflow_error("glTF float range exceeded");
    const auto value = static_cast<float>(v);
    if (!std::isfinite(value))
        throw std::overflow_error("glTF float range exceeded");
    return value;
}
void f32(std::vector<std::uint8_t> &out, double value)
{
    u32(out, std::bit_cast<std::uint32_t>(narrow(value)));
}
void align(std::vector<std::uint8_t> &out)
{
    while (out.size() % 4)
        out.push_back(0);
}
std::size_t Buffer::view(std::vector<std::uint8_t> data, unsigned target)
{
    align(bytes);
    auto s = stream();
    s << "{\"buffer\":0,\"byteOffset\":" << bytes.size() << ",\"byteLength\":" << data.size();
    if (target)
        s << ",\"target\":" << target;
    s << '}';
    views.push_back(s.str());
    bytes.insert(bytes.end(), data.begin(), data.end());
    return views.size() - 1;
}

std::size_t Buffer::accessor(std::size_t viewIndex, unsigned component, std::size_t count,
                     std::string_view type, std::string extra)
{
    if (viewIndex >= views.size()) throw std::out_of_range("glTF buffer view index");
    auto s = stream();
    s << "{\"bufferView\":" << viewIndex << ",\"componentType\":" << component << ",\"count\":" << count
      << ",\"type\":" << quote(type) << extra << '}';
    accessors.push_back(s.str());
    return accessors.size() - 1;
}

std::size_t Buffer::vectors(std::span<const Vector3> values, bool bounds)
{
    std::vector<std::uint8_t> data;
    std::array<float, 3> low{}, high{};
    bool first = true;
    for (auto p : values)
    {
        const std::array<float, 3> v{narrow(p.x), narrow(p.y), narrow(p.z)};
        for (unsigned i = 0; i < 3; ++i)
        {
            if (first)
                low[i] = high[i] = v[i];
            else
            {
                low[i] = std::min(low[i], v[i]);
                high[i] = std::max(high[i], v[i]);
            }
            f32(data, v[i]);
        }
        first = false;
    }
    std::string extra;
    if (bounds)
    {
        auto s = stream();
        s << ",\"min\":[" << low[0] << ',' << low[1] << ',' << low[2] << "],\"max\":[" << high[0] << ','
          << high[1] << ',' << high[2] << ']';
        extra = s.str();
    }
    return accessor(view(std::move(data), 34962), 5126, values.size(), "VEC3", extra);
}

std::size_t Buffer::sparse(const MorphTarget &morph, std::size_t count, bool normal)
{
    std::vector<MorphDelta> deltas = morph.deltas;
    std::sort(deltas.begin(), deltas.end(),
              [](const auto &a, const auto &b) { return a.vertex < b.vertex; });
    for (std::size_t i=0; i<deltas.size(); ++i)
        if (deltas[i].vertex >= count || deltas[i].vertex > std::numeric_limits<std::uint32_t>::max() ||
            (i && deltas[i-1].vertex == deltas[i].vertex))
            throw std::invalid_argument("Invalid glTF sparse morph vertex index");
    std::vector<std::uint8_t> indices, values;
    std::array<float, 3> low{}, high{}; // Include implicit zero deltas.
    bool first = deltas.size() == count;
    for (const auto &delta : deltas)
    {
        const auto p = normal ? delta.normal : delta.position;
        u32(indices, static_cast<std::uint32_t>(delta.vertex));
        const std::array<float, 3> v{narrow(p.x), narrow(p.y), narrow(p.z)};
        for (unsigned i = 0; i < 3; ++i)
        {
            f32(values, v[i]);
            if (first)
                low[i] = high[i] = v[i];
            else
            {
                low[i] = std::min(low[i], v[i]);
                high[i] = std::max(high[i], v[i]);
            }
        }
        first = false;
    }
    auto s = stream();
    s << "{\"componentType\":5126,\"count\":" << count << ",\"type\":\"VEC3\"";
    if (!normal)
        s << ",\"min\":[" << low[0] << ',' << low[1] << ',' << low[2] << "],\"max\":[" << high[0] << ','
          << high[1] << ',' << high[2] << ']';
    if (!deltas.empty())
    {
        const auto iv = view(std::move(indices)), vv = view(std::move(values));
        s << ",\"sparse\":{\"count\":" << deltas.size() << ",\"indices\":{\"bufferView\":" << iv
          << ",\"componentType\":5125},\"values\":{\"bufferView\":" << vv << "}}";
    }
    s << '}';
    accessors.push_back(s.str());
    return accessors.size() - 1;
}
void jsonArray(std::ostream &out, std::span<const std::string> values)
{
    out << '[';
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (i)
            out << ',';
        out << values[i];
    }
    out << ']';
}

}
