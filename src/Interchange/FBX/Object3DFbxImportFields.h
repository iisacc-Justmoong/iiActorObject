#pragma once
#include "../third_party/ufbx/ufbx.h"
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <string_view>

namespace iiActorObject::object3d_detail
{
using FbxWarningSink = std::function<void(std::string)>;
} // namespace iiActorObject::object3d_detail

namespace iiActorObject::object3d_detail::fbx_fields
{
std::string string(ufbx_string value);
const ufbx_prop *property(const ufbx_props &props,std::string_view name);
std::uint32_t externalIndex(const ufbx_prop *property);
std::string_view externalText(const ufbx_prop *property);
std::string unique(std::string name,std::set<std::string> &used);
} // namespace iiActorObject::object3d_detail::fbx_fields
