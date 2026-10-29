#include "Object3DFbxMetadata.h"
#include <limits>
#include <stdexcept>

namespace iiActorObject::object3d_detail::fbx_metadata
{
namespace
{
std::string string(ufbx_string value){return {value.data?value.data:"",value.length};}
}
const ufbx_dom_node *child(const ufbx_dom_node *node,const char *name)
{return node?ufbx_dom_find(node,name):nullptr;}
const ufbx_dom_node *uniqueChild(const ufbx_dom_node *node,std::string_view name)
{
    const ufbx_dom_node *result=nullptr;
    if(node)for(const auto *entry:node->children)
        if(std::string_view(entry->name.data,entry->name.length)==name)
        {
            if(result)throw std::invalid_argument("Duplicate FBX authoring metadata field");
            result=entry;
        }
    return result;
}
const ufbx_dom_value &scalar(const ufbx_dom_node *node)
{
    if(!node||node->values.count!=1||!node->values.data)throw std::invalid_argument("Invalid FBX authoring metadata scalar");
    return node->values[0];
}
std::uint32_t number(const ufbx_dom_node *node)
{
    const auto &v=scalar(node);
    if(v.type!=UFBX_DOM_VALUE_NUMBER||v.value_int<0||static_cast<std::uint64_t>(v.value_int)>std::numeric_limits<std::uint32_t>::max()||
       v.value_float!=static_cast<double>(v.value_int))
        throw std::invalid_argument("Invalid FBX authoring metadata number");
    return static_cast<std::uint32_t>(v.value_int);
}
std::string text(const ufbx_dom_node *node)
{
    const auto &v=scalar(node);
    if(v.type!=UFBX_DOM_VALUE_STRING||v.value_str.length>1024||(!v.value_str.data&&v.value_str.length))
        throw std::invalid_argument("Invalid FBX authoring metadata text");
    return string(v.value_str);
}
std::vector<std::uint32_t> array(const ufbx_dom_node *node,std::size_t limit)
{
    const auto &v=scalar(node);const auto b=v.value_blob;
    if(v.type!=UFBX_DOM_VALUE_STRING||b.size%4||b.size/4>limit||(!b.data&&b.size))
        throw std::invalid_argument("Invalid FBX authoring metadata array or limit: "+(node?string(node->name):std::string("missing")));
    const auto *bytes=static_cast<const std::uint8_t*>(b.data);std::vector<std::uint32_t> result;
    for(std::size_t i=0;i<b.size;i+=4)
    {std::uint32_t value=0;for(unsigned k=0;k<4;++k)value|=static_cast<std::uint32_t>(bytes[i+k])<<(k*8);result.push_back(value);}
    return result;
}
} // namespace iiActorObject::object3d_detail::fbx_metadata
