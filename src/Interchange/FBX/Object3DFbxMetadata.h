#pragma once
#include "../third_party/ufbx/ufbx.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace iiActorObject::object3d_detail::fbx_metadata
{
[[nodiscard]] const ufbx_dom_node *child(const ufbx_dom_node *node,const char *name);
[[nodiscard]] const ufbx_dom_node *uniqueChild(const ufbx_dom_node *node,std::string_view name);
[[nodiscard]] const ufbx_dom_value &scalar(const ufbx_dom_node *node);
[[nodiscard]] std::uint32_t number(const ufbx_dom_node *node);
[[nodiscard]] std::string text(const ufbx_dom_node *node);
[[nodiscard]] std::vector<std::uint32_t> array(const ufbx_dom_node *node,std::size_t limit);
} // namespace iiActorObject::object3d_detail::fbx_metadata
