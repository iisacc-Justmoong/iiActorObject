#include <iiActorObject/GlbDocument.h>
#include <iiJson/Json.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace iiActorObject {
namespace {
using Value = ii::json::Value;
using Array = Value::Array;

[[noreturn]] void invalid(std::string_view message)
{
    throw std::invalid_argument("GLB: " + std::string(message));
}

const Value::Object& object(const Value& value)
{
    const auto result = value.get<Value::Object>();
    if (!result) invalid("expected an object");
    return result->get();
}
const Value* member(const Value& value, std::string_view key)
{
    const auto& entries = object(value);
    const auto it = entries.find(key);
    return it == entries.end() ? nullptr : &it->second;
}
const Value& required(const Value& value, std::string_view key)
{
    const auto* result = member(value, key);
    if (!result) invalid("missing required property: " + std::string(key));
    return *result;
}
const Array& array(const Value& value)
{
    const auto result = value.get<Array>();
    if (!result) invalid("expected an array");
    return result->get();
}
std::string_view string(const Value& value)
{
    const auto result = value.get<std::string>();
    if (!result) invalid("expected a string");
    return result->get();
}
std::size_t number(const Value& value)
{
    std::uint64_t result = 0;
    if (const auto* u = std::get_if<std::uint64_t>(&value.storage())) result = *u;
    else if (const auto* s = std::get_if<std::int64_t>(&value.storage()); s && *s >= 0)
        result = static_cast<std::uint64_t>(*s);
    else invalid("expected a nonnegative integer");
    if (result > std::numeric_limits<std::size_t>::max()) invalid("integer exceeds platform size");
    return static_cast<std::size_t>(result);
}
std::size_t optionalNumber(const Value& value, std::string_view key, std::size_t fallback = 0)
{
    const auto* entry = member(value, key);
    return entry ? number(*entry) : fallback;
}
bool optionalBoolean(const Value& value, std::string_view key)
{
    const auto* entry = member(value, key);
    if (!entry) return false;
    const auto result = entry->get<bool>();
    if (!result) invalid("expected a boolean");
    return result->get();
}
std::uint32_t u32(std::span<const std::uint8_t> data, std::size_t offset)
{
    if (offset > data.size() || data.size() - offset < 4) invalid("truncated integer");
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}
std::size_t componentBytes(GlbComponentType type)
{
    switch (type) {
    case GlbComponentType::Byte: case GlbComponentType::UnsignedByte: return 1;
    case GlbComponentType::Short: case GlbComponentType::UnsignedShort: return 2;
    case GlbComponentType::UnsignedInt: case GlbComponentType::Float: return 4;
    }
    invalid("unsupported component type");
}
GlbComponentType component(const Value& value)
{
    const auto id = number(value);
    switch (id) {
    case 5120: return GlbComponentType::Byte;
    case 5121: return GlbComponentType::UnsignedByte;
    case 5122: return GlbComponentType::Short;
    case 5123: return GlbComponentType::UnsignedShort;
    case 5125: return GlbComponentType::UnsignedInt;
    case 5126: return GlbComponentType::Float;
    default: invalid("unsupported component type");
    }
}
struct Shape { GlbAccessorType type; std::size_t columns; std::size_t rows; };
Shape shape(std::string_view type)
{
    if (type == "SCALAR") return {GlbAccessorType::Scalar, 1, 1};
    if (type == "VEC2") return {GlbAccessorType::Vec2, 1, 2};
    if (type == "VEC3") return {GlbAccessorType::Vec3, 1, 3};
    if (type == "VEC4") return {GlbAccessorType::Vec4, 1, 4};
    if (type == "MAT2") return {GlbAccessorType::Mat2, 2, 2};
    if (type == "MAT3") return {GlbAccessorType::Mat3, 3, 3};
    if (type == "MAT4") return {GlbAccessorType::Mat4, 4, 4};
    invalid("unsupported accessor shape");
}
struct View { std::size_t offset; std::size_t length; std::size_t stride; };
struct Sparse {
    std::vector<std::size_t> indices;
    std::size_t valuesOffset;
};
struct Accessor {
    GlbAccessorInfo info;
    std::size_t rows;
    std::size_t columnStride;
    std::size_t elementBytes;
    std::optional<std::size_t> baseOffset;
    std::size_t baseStride;
    std::optional<Sparse> sparse;
};
void range(std::size_t offset, std::size_t count, std::size_t stride,
           std::size_t element, std::size_t size)
{
    if (!count || !stride || offset > size || element > size - offset ||
        count - 1 > (size - offset - element) / stride)
        invalid("accessor exceeds its buffer view");
}
std::uint32_t integerAt(const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t width)
{
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
        value |= static_cast<std::uint32_t>(data[offset + i]) << (8 * i);
    return value;
}
double valueAt(const std::vector<std::uint8_t>& bytes, std::size_t offset,
               GlbComponentType type, bool normalized)
{
    const auto raw = integerAt(bytes, offset, componentBytes(type));
    switch (type) {
    case GlbComponentType::Byte: {
        const auto value = std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(raw));
        return normalized ? std::max(-1.0, value / 127.0) : value;
    }
    case GlbComponentType::UnsignedByte: return normalized ? raw / 255.0 : raw;
    case GlbComponentType::Short: {
        const auto value = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(raw));
        return normalized ? std::max(-1.0, value / 32767.0) : value;
    }
    case GlbComponentType::UnsignedShort: return normalized ? raw / 65535.0 : raw;
    case GlbComponentType::UnsignedInt: return raw;
    case GlbComponentType::Float: {
        const auto value = std::bit_cast<float>(raw);
        if (!std::isfinite(value)) invalid("non-finite accessor value");
        return value;
    }
    }
    invalid("unsupported component type");
}
std::size_t componentOffset(const Accessor& accessor, std::size_t componentIndex)
{
    return (componentIndex / accessor.rows) * accessor.columnStride +
           (componentIndex % accessor.rows) * componentBytes(accessor.info.component);
}
} // namespace

struct GlbDocument::Impl {
    std::string json;
    std::vector<std::uint8_t> bytes;
    std::vector<View> views;
    std::vector<Accessor> accessors;
};

GlbDocument GlbDocument::parse(std::span<const std::uint8_t> bytes, const GlbReadOptions& options)
{
    if (bytes.size() > options.maximumBytes || bytes.size() < 20) invalid("container size outside limit");
    if (u32(bytes, 0) != 0x46546c67 || u32(bytes, 4) != 2 || u32(bytes, 8) != bytes.size())
        invalid("invalid container header");
    std::span<const std::uint8_t> jsonBytes, binBytes;
    std::size_t position = 12, chunkIndex = 0;
    bool foundBin = false;
    while (position < bytes.size()) {
        if (bytes.size() - position < 8) invalid("truncated chunk header");
        const auto length = static_cast<std::size_t>(u32(bytes, position));
        const auto type = u32(bytes, position + 4);
        position += 8;
        if (length % 4 || length > bytes.size() - position) invalid("invalid chunk length");
        const auto chunk = bytes.subspan(position, length);
        if (chunkIndex == 0) {
            if (type != 0x4e4f534a || chunk.empty() || length > options.maximumJsonBytes)
                invalid("invalid JSON chunk");
            jsonBytes = chunk;
        } else if (type == 0x004e4942) {
            if (foundBin || chunkIndex != 1) invalid("invalid BIN chunk placement");
            foundBin = true;
            binBytes = chunk;
        } else if (type == 0x4e4f534a) invalid("duplicate JSON chunk");
        position += length;
        ++chunkIndex;
    }

    auto result = std::make_shared<Impl>();
    result->json.assign(reinterpret_cast<const char*>(jsonBytes.data()), jsonBytes.size());
    ii::json::Limits limits;
    limits.input_bytes = options.maximumJsonBytes;
    limits.string_bytes = options.maximumJsonBytes;
    limits.items = options.maximumJsonItems;
    const auto parsed = ii::json::parse(result->json, limits);
    if (!parsed) invalid("invalid or oversized JSON: " + parsed.error().message);
    const auto& root = *parsed;
    if (string(required(required(root, "asset"), "version")) != "2.0") invalid("unsupported asset version");

    std::size_t bufferLength = 0;
    if (const auto* buffers = member(root, "buffers")) {
        const auto& entries = array(*buffers);
        if (entries.size() > 1) invalid("external or multiple buffers are unsupported");
        if (!entries.empty()) {
            const auto& buffer = entries.front();
            if (member(buffer, "uri")) invalid("external and data URI buffers are unsupported");
            bufferLength = number(required(buffer, "byteLength"));
            if (!foundBin || bufferLength > binBytes.size() || binBytes.size() - bufferLength > 3)
                invalid("declared buffer length does not match BIN chunk");
            result->bytes.assign(binBytes.begin(), binBytes.begin() + bufferLength);
        }
    }
    if (foundBin && bufferLength == 0) invalid("BIN chunk has no declared nonempty buffer");

    if (const auto* views = member(root, "bufferViews")) {
        const auto& entries = array(*views);
        if (entries.size() > options.maximumBufferViews) invalid("too many buffer views");
        result->views.reserve(entries.size());
        for (const auto& entry : entries) {
            if (number(required(entry, "buffer")) != 0 || !bufferLength) invalid("invalid buffer reference");
            const auto offset = optionalNumber(entry, "byteOffset");
            const auto length = number(required(entry, "byteLength"));
            const auto stride = optionalNumber(entry, "byteStride");
            if (!length || offset > bufferLength || length > bufferLength - offset) invalid("buffer view exceeds buffer");
            if (member(entry, "byteStride") && (stride < 4 || stride > 252 || stride % 4)) invalid("invalid buffer view stride");
            result->views.push_back({offset, length, stride});
        }
    }
    auto getView = [&](std::size_t index) -> const View& {
        if (index >= result->views.size()) invalid("invalid buffer view reference");
        return result->views[index];
    };
    std::size_t totalValues = 0, totalSparse = 0;
    if (const auto* accessors = member(root, "accessors")) {
        const auto& entries = array(*accessors);
        if (entries.size() > options.maximumAccessors) invalid("too many accessors");
        result->accessors.reserve(entries.size());
        for (const auto& entry : entries) {
            const auto type = component(required(entry, "componentType"));
            const auto s = shape(string(required(entry, "type")));
            const auto count = number(required(entry, "count"));
            const auto components = s.columns * s.rows;
            const auto width = componentBytes(type);
            const auto normalized = optionalBoolean(entry, "normalized");
            if (normalized && (type == GlbComponentType::Float || type == GlbComponentType::UnsignedInt))
                invalid("invalid normalized component type");
            if (!count || totalValues > options.maximumAccessorValues ||
                count > (options.maximumAccessorValues - totalValues) / components)
                invalid("accessor value limit exceeded");
            totalValues += count * components;
            const auto columnStride = s.columns == 1 ? s.rows * width : (s.rows * width + 3) & ~std::size_t{3};
            const auto elementBytes = s.columns * columnStride;
            Accessor accessor{{count, components, type, s.type, normalized}, s.rows, columnStride, elementBytes, {}, elementBytes, {}};
            const auto offset = optionalNumber(entry, "byteOffset");
            if (const auto* viewIndex = member(entry, "bufferView")) {
                const auto& view = getView(number(*viewIndex));
                if (offset % width || (view.offset + offset) % width ||
                    (s.columns > 1 && width < 4 && (view.offset + offset) % 4))
                    invalid("misaligned accessor");
                const auto stride = view.stride ? view.stride : elementBytes;
                if (stride < elementBytes || stride % width) invalid("invalid accessor stride");
                range(offset, count, stride, elementBytes, view.length);
                accessor.baseOffset = view.offset + offset;
                accessor.baseStride = stride;
            } else if (offset) invalid("offset without a buffer view");

            if (const auto* sparseValue = member(entry, "sparse")) {
                const auto sparseCount = number(required(*sparseValue, "count"));
                if (!sparseCount || sparseCount > count || totalSparse > options.maximumSparseEntries ||
                    sparseCount > options.maximumSparseEntries - totalSparse) invalid("invalid sparse count");
                totalSparse += sparseCount;
                const auto& indices = required(*sparseValue, "indices");
                const auto indexType = component(required(indices, "componentType"));
                if (indexType != GlbComponentType::UnsignedByte && indexType != GlbComponentType::UnsignedShort &&
                    indexType != GlbComponentType::UnsignedInt) invalid("invalid sparse index type");
                const auto indexWidth = componentBytes(indexType);
                const auto& indexView = getView(number(required(indices, "bufferView")));
                const auto indexOffset = optionalNumber(indices, "byteOffset");
                if (indexView.stride || indexOffset % indexWidth || (indexView.offset + indexOffset) % indexWidth)
                    invalid("invalid sparse index layout");
                range(indexOffset, sparseCount, indexWidth, indexWidth, indexView.length);
                const auto& values = required(*sparseValue, "values");
                const auto& valueView = getView(number(required(values, "bufferView")));
                const auto valueOffset = optionalNumber(values, "byteOffset");
                if (valueView.stride || valueOffset % width || (valueView.offset + valueOffset) % width ||
                    (s.columns > 1 && width < 4 && (valueView.offset + valueOffset) % 4))
                    invalid("invalid sparse value layout");
                range(valueOffset, sparseCount, elementBytes, elementBytes, valueView.length);
                Sparse sparse{{}, valueView.offset + valueOffset};
                sparse.indices.reserve(sparseCount);
                for (std::size_t i = 0; i < sparseCount; ++i) {
                    const auto index = integerAt(result->bytes, indexView.offset + indexOffset + i * indexWidth, indexWidth);
                    if (index >= count || (!sparse.indices.empty() && index <= sparse.indices.back()))
                        invalid("sparse indices must be strictly increasing and in range");
                    sparse.indices.push_back(index);
                }
                accessor.sparse = std::move(sparse);
            }
            result->accessors.push_back(std::move(accessor));
        }
    }
    return GlbDocument(std::move(result));
}

std::size_t GlbDocument::accessorCount() const noexcept { return impl_->accessors.size(); }
std::size_t GlbDocument::bufferViewCount() const noexcept { return impl_->views.size(); }
std::span<const std::uint8_t> GlbDocument::bufferView(std::size_t index) const
{
    const auto& view = impl_->views.at(index);
    return std::span<const std::uint8_t>(impl_->bytes).subspan(view.offset, view.length);
}
GlbAccessorInfo GlbDocument::accessor(std::size_t index) const { return impl_->accessors.at(index).info; }
std::string_view GlbDocument::json() const noexcept { return impl_->json; }

std::vector<double> GlbDocument::readFloats(std::size_t index) const
{
    const auto& a = impl_->accessors.at(index);
    std::vector<double> result(a.info.count * a.info.components, 0);
    const auto copy = [&](std::size_t destination, std::size_t offset) {
        for (std::size_t c = 0; c < a.info.components; ++c)
            result[destination * a.info.components + c] = valueAt(impl_->bytes, offset + componentOffset(a, c), a.info.component, a.info.normalized);
    };
    if (a.baseOffset) for (std::size_t i = 0; i < a.info.count; ++i) copy(i, *a.baseOffset + i * a.baseStride);
    if (a.sparse) for (std::size_t i = 0; i < a.sparse->indices.size(); ++i) copy(a.sparse->indices[i], a.sparse->valuesOffset + i * a.elementBytes);
    return result;
}
std::vector<std::uint32_t> GlbDocument::readUnsigned(std::size_t index) const
{
    const auto& a = impl_->accessors.at(index);
    if (a.info.normalized || (a.info.component != GlbComponentType::UnsignedByte &&
        a.info.component != GlbComponentType::UnsignedShort && a.info.component != GlbComponentType::UnsignedInt))
        invalid("accessor is not an unnormalized unsigned integer");
    std::vector<std::uint32_t> result(a.info.count * a.info.components, 0);
    const auto copy = [&](std::size_t destination, std::size_t offset) {
        for (std::size_t c = 0; c < a.info.components; ++c)
            result[destination * a.info.components + c] = integerAt(impl_->bytes, offset + componentOffset(a, c), componentBytes(a.info.component));
    };
    if (a.baseOffset) for (std::size_t i = 0; i < a.info.count; ++i) copy(i, *a.baseOffset + i * a.baseStride);
    if (a.sparse) for (std::size_t i = 0; i < a.sparse->indices.size(); ++i) copy(a.sparse->indices[i], a.sparse->valuesOffset + i * a.elementBytes);
    return result;
}
} // namespace iiActorObject
