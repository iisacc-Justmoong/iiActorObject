#include <iiActorObject/Usd.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace iiActorObject {
namespace {
bool identifier(std::string_view name)
{
    const auto letter = [](unsigned char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
    };
    if (name.empty() || !letter(static_cast<unsigned char>(name.front()))) return false;
    return std::all_of(name.begin(), name.end(), [&](unsigned char c) {
        return letter(c) || (c >= '0' && c <= '9');
    });
}

bool absolutePrimPath(std::string_view path)
{
    if (path.empty() || path.front() != '/' || path.size() == 1 || path.back() == '/') return false;
    path.remove_prefix(1);
    while (!path.empty()) {
        const auto slash = path.find('/');
        if (!identifier(path.substr(0, slash))) return false;
        if (slash == std::string_view::npos) return true;
        path.remove_prefix(slash + 1);
    }
    return false;
}

void utf8(std::string_view text)
{
    for (std::size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i]);
        if (lead == 0) throw std::invalid_argument("USD text cannot contain NUL");
        if (lead < 0x80) { ++i; continue; }
        std::size_t count = 0;
        unsigned codepoint = 0, minimum = 0;
        if (lead >= 0xc2 && lead <= 0xdf) { count = 2; codepoint = lead & 0x1f; minimum = 0x80; }
        else if (lead >= 0xe0 && lead <= 0xef) { count = 3; codepoint = lead & 0x0f; minimum = 0x800; }
        else if (lead >= 0xf0 && lead <= 0xf4) { count = 4; codepoint = lead & 0x07; minimum = 0x10000; }
        else throw std::invalid_argument("USD text must be valid UTF-8");
        if (text.size() - i < count) throw std::invalid_argument("Truncated UTF-8 in USD text");
        for (std::size_t j = 1; j < count; ++j) {
            const auto next = static_cast<unsigned char>(text[i + j]);
            if ((next & 0xc0) != 0x80) throw std::invalid_argument("Invalid UTF-8 continuation");
            codepoint = (codepoint << 6) | (next & 0x3f);
        }
        if (codepoint < minimum || codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff))
            throw std::invalid_argument("Invalid UTF-8 code point");
        i += count;
    }
}

std::string quoted(std::string_view text)
{
    utf8(text);
    constexpr char hex[] = "0123456789abcdef";
    std::string result = "\"";
    for (const unsigned char c : text) {
        switch (c) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (c < 0x20 || c == 0x7f) {
                result += "\\x";
                result += hex[c >> 4];
                result += hex[c & 0xf];
            } else result += static_cast<char>(c);
        }
    }
    return result + '"';
}

std::string number(double value)
{
    if (!std::isfinite(value)) throw std::invalid_argument("USD numeric values must be finite");
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof buffer, value,
        std::chars_format::general, std::numeric_limits<double>::max_digits10);
    if (result.ec != std::errc{}) throw std::invalid_argument("USD numeric value is not representable");
    return {buffer, result.ptr};
}

std::string assetLiteral(std::string path)
{
    utf8(path);
    if (path.empty() || path.find("@@@") != std::string::npos ||
        std::any_of(path.begin(), path.end(), [](unsigned char c) { return c < 0x20 || c == 0x7f; }))
        throw std::invalid_argument("USD asset path is empty or contains unsupported delimiters/control characters");
    std::replace(path.begin(), path.end(), '\\', '/');
    return path.find('@') == std::string::npos ? '@' + path + '@' : "@@@" + path + "@@@";
}

std::string_view stateName(LifecycleState state)
{
    switch (state) {
    case LifecycleState::Created: return "created";
    case LifecycleState::Playing: return "playing";
    case LifecycleState::Stopped: return "stopped";
    case LifecycleState::Destroyed: return "destroyed";
    }
    throw std::invalid_argument("Unknown Actor lifecycle state");
}

std::string boolean(bool value) { return value ? "true" : "false"; }

struct Node {
    const ActorComponent* component = nullptr;
    const SceneComponent* scene = nullptr;
    std::string name, path;
    std::vector<std::size_t> children, references;
    bool reset = false;
};

struct Snapshot {
    std::vector<Node> nodes;
    UsdActorBinding binding;
};

Snapshot snapshot(const UsdActorPlacement& placement, std::string name, std::string rootPath)
{
    Snapshot result;
    const auto& actor = *placement.actor;
    const auto components = actor.components();
    result.nodes.resize(components.size());
    result.binding = {actor.id(), rootPath + '/' + name, {}, {}};
    result.binding.components.resize(components.size());
    result.binding.assets.resize(placement.references.size());
    std::unordered_map<const ActorComponent*, std::size_t> indices;
    for (std::size_t i = 0; i < components.size(); ++i) {
        auto& node = result.nodes[i];
        node.component = components[i];
        node.scene = dynamic_cast<const SceneComponent*>(components[i]);
        node.name = i == 0 ? name : "Component_" + std::to_string(i);
        indices.emplace(components[i], i);
    }
    for (std::size_t i = 1; i < result.nodes.size(); ++i) {
        auto& node = result.nodes[i];
        std::size_t parent = 0;
        if (node.scene && node.scene->parent()) {
            const auto found = indices.find(node.scene->parent());
            if (found == indices.end()) throw std::invalid_argument("USD scene parent is outside its Actor");
            parent = found->second;
        } else if (node.scene) node.reset = true;
        result.nodes[parent].children.push_back(i);
    }
    result.nodes[0].path = result.binding.primPath;
    std::vector<std::size_t> queue{0};
    for (std::size_t cursor = 0; cursor < queue.size(); ++cursor) {
        const auto index = queue[cursor];
        const auto& node = result.nodes[index];
        result.binding.components[index] = {index, node.path};
        for (const auto child : node.children) {
            result.nodes[child].path = node.path + '/' + result.nodes[child].name;
            queue.push_back(child);
        }
    }
    if (queue.size() != components.size()) throw std::invalid_argument("USD scene hierarchy is disconnected or cyclic");
    for (std::size_t i = 0; i < placement.references.size(); ++i) {
        const auto& reference = placement.references[i];
        const auto* target = reference.component ? reference.component : &actor.rootComponent();
        const auto found = indices.find(target);
        if (found == indices.end()) throw std::invalid_argument("USD asset target is outside its Actor");
        if (!reference.assetPrimPath.empty() && !absolutePrimPath(reference.assetPrimPath))
            throw std::invalid_argument("USD asset prim path must be an absolute ASCII prim path");
        (void)assetLiteral(reference.assetPath);
        auto& node = result.nodes[found->second];
        node.references.push_back(i);
        result.binding.assets[i] = {i, node.path + "/Asset_" + std::to_string(i)};
    }
    return result;
}

class Writer {
public:
    std::string text;
    void line(std::size_t depth, const std::string& value)
    {
        text.append(depth * 4, ' ');
        text += value;
        text += '\n';
    }
    void transform(std::size_t depth, const SceneComponent& scene, bool reset)
    {
        const auto rows = toUsdMatrix(scene.localTransform().matrix());
        std::string value = "matrix4d xformOp:transform = (";
        for (std::size_t row = 0; row < 4; ++row) {
            if (row) value += ", ";
            value += '(';
            for (std::size_t column = 0; column < 4; ++column) {
                if (column) value += ", ";
                value += number(rows[row][column]);
            }
            value += ')';
        }
        line(depth, value + ')');
        line(depth, std::string("uniform token[] xformOpOrder = [") +
            (reset ? "\"!resetXformStack!\", " : "") + "\"xformOp:transform\"]");
    }
    void component(std::size_t depth, const Node& node, std::size_t index)
    {
        line(depth, "custom uint64 iiActor:componentIndex = " + std::to_string(index));
        line(depth, "custom string iiActor:componentName = " + quoted(node.component->name()));
        line(depth, "custom token iiActor:componentType = " + quoted(node.scene ? "scene" : "behavior"));
        line(depth, "custom token iiActor:componentState = " + quoted(stateName(node.component->state())));
        line(depth, "custom bool iiActor:componentActive = " + boolean(node.component->isActive()));
        line(depth, "custom bool iiActor:componentTickEnabled = " + boolean(node.component->isTickEnabled()));
    }
    void actor(std::size_t depth, const Actor& actor)
    {
        line(depth, "custom uint64 iiActor:id = " + std::to_string(actor.id().value));
        line(depth, "custom string iiActor:name = " + quoted(actor.name()));
        line(depth, "custom token iiActor:state = " + quoted(stateName(actor.state())));
        line(depth, "custom bool iiActor:tickEnabled = " + boolean(actor.isTickEnabled()));
        std::string tags = "custom string[] iiActor:tags = [";
        for (const auto& tag : actor.tags()) {
            if (tags.back() != '[') tags += ", ";
            tags += quoted(tag);
        }
        line(depth, tags + ']');
    }
    void asset(std::size_t depth, std::size_t index, const UsdAssetReference& reference)
    {
        // Untyped definition preserves a referenced Mesh/Sphere/Xform root's schema.
        line(depth, "def \"Asset_" + std::to_string(index) + "\" (");
        auto arc = "prepend references = " + assetLiteral(reference.assetPath);
        if (!reference.assetPrimPath.empty()) arc += '<' + reference.assetPrimPath + '>';
        line(depth + 1, arc);
        line(depth + 1, "instanceable = " + boolean(reference.instanceable));
        line(depth, ")");
        line(depth, "{");
        line(depth, "}");
    }
    void placement(const UsdActorPlacement& placement, const Snapshot& snapshot)
    {
        struct Frame { std::size_t index, depth; bool close; };
        std::vector<Frame> stack{{0, 1, false}};
        while (!stack.empty()) {
            const auto frame = stack.back();
            stack.pop_back();
            if (frame.close) { line(frame.depth, "}"); continue; }
            const auto& node = snapshot.nodes[frame.index];
            line(frame.depth, std::string("def ") + (node.scene ? "Xform " : "Scope ") + quoted(node.name) + " (");
            line(frame.depth + 1, "displayName = " + quoted(frame.index == 0 ? placement.actor->name() : node.component->name()));
            line(frame.depth, ")");
            line(frame.depth, "{");
            if (frame.index == 0) actor(frame.depth + 1, *placement.actor);
            component(frame.depth + 1, node, frame.index);
            if (node.scene) transform(frame.depth + 1, *node.scene, node.reset);
            for (const auto reference : node.references)
                asset(frame.depth + 1, reference, placement.references[reference]);
            stack.push_back({frame.index, frame.depth, true});
            for (auto child = node.children.rbegin(); child != node.children.rend(); ++child)
                stack.push_back({*child, frame.depth + 1, false});
        }
    }
};
} // namespace

UsdMatrix4 toUsdMatrix(const Matrix4& matrix)
{
    UsdMatrix4 result{};
    for (std::size_t row = 0; row < 4; ++row)
        for (std::size_t column = 0; column < 4; ++column) {
            const auto value = matrix.elements()[row * 4 + column];
            if (!std::isfinite(value)) throw std::invalid_argument("USD transform must be finite");
            result[row][column] = value;
        }
    return result;
}

UsdLayer exportUsdLayer(std::span<const UsdActorPlacement> placements, const UsdStageOptions& options)
{
    if (!identifier(options.rootPrimName)) throw std::invalid_argument("USD root must be an ASCII prim identifier");
    if (!std::isfinite(options.metersPerUnit) || options.metersPerUnit <= 0)
        throw std::invalid_argument("USD metersPerUnit must be finite and positive");
    if (options.upAxis != UsdUpAxis::Y && options.upAxis != UsdUpAxis::Z)
        throw std::invalid_argument("USD up axis must be Y or Z");
    Writer writer;
    writer.line(0, "#usda 1.0");
    writer.line(0, "(");
    writer.line(1, "defaultPrim = " + quoted(options.rootPrimName));
    writer.line(1, std::string("upAxis = \"") + (options.upAxis == UsdUpAxis::Y ? "Y" : "Z") + '"');
    writer.line(1, "metersPerUnit = " + number(options.metersPerUnit));
    writer.line(0, ")");
    writer.line(0, "def Xform " + quoted(options.rootPrimName));
    writer.line(0, "{");
    UsdLayer result;
    std::unordered_set<std::uint64_t> ids;
    std::unordered_set<std::string> names;
    for (const auto& placement : placements) {
        if (!placement.actor || placement.actor->isDestroyed())
            throw std::invalid_argument("USD placement requires a live Actor");
        const auto id = placement.actor->id();
        const auto name = placement.primName.empty() ? "Actor_" + std::to_string(id.value) : placement.primName;
        if (!identifier(name) || !names.insert(name).second || !ids.insert(id.value).second)
            throw std::invalid_argument("USD Actor placement names and IDs must be valid and unique");
        auto captured = snapshot(placement, name, '/' + options.rootPrimName);
        writer.placement(placement, captured);
        result.actors.push_back(std::move(captured.binding));
    }
    writer.line(0, "}");
    result.usda = std::move(writer.text);
    return result;
}
} // namespace iiActorObject
