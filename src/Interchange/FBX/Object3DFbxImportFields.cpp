#include "Object3DFbxImportFields.h"
#include <limits>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail::fbx_fields
{
std::string string(ufbx_string s)
{
    if(!s.data&&s.length)throw std::invalid_argument("Invalid FBX string");
    return {s.data ? s.data : "", s.length};
}
const ufbx_prop *property(const ufbx_props &props, std::string_view name)
{
    for (const auto &p : props.props)
        if (std::string_view(p.name.data, p.name.length) == name)
            return &p;
    return nullptr;
}
std::uint32_t externalIndex(const ufbx_prop *p)
{if(!p||p->type!=UFBX_PROP_INTEGER||p->value_int<0||static_cast<std::uint64_t>(p->value_int)>=std::numeric_limits<std::uint32_t>::max())throw std::invalid_argument("Malformed external source index property");return static_cast<std::uint32_t>(p->value_int);}
std::string_view externalText(const ufbx_prop *p)
{if(!p||p->type!=UFBX_PROP_STRING||(!p->value_str.data&&p->value_str.length))throw std::invalid_argument("Missing external identity text property");return {p->value_str.data?p->value_str.data:"",p->value_str.length};}
std::string unique(std::string name, std::set<std::string> &used)
{
    if (name.empty())
        name = "Unnamed";
    const auto base = name;
    unsigned suffix = 1;
    while (!used.insert(name).second)
        name = base + "_" + std::to_string(suffix++);
    return name;
}
} // namespace iiActorObject::object3d_detail::fbx_fields
