#include "HumanAccessoryCatalogInternal.h"
#include <iiActorObject/Object3DNames.h>
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>

namespace iiActorObject::NSCharacter::human_detail
{
std::vector<AccessoryEntry> accessoryCatalog(Files &files)
{
    const auto catalog=files.json("catalog.json");
    if(as<std::string>(field(catalog,"format"))!="iiActorObject.MakeHumanWardrobe.1"||as<std::string>(field(catalog,"baseMesh"))!="hm08")
        throw std::invalid_argument("Unsupported human accessory catalogue");
    const auto &components=as<Value::Array>(field(catalog,"components"));if(components.empty()||components.size()>512)throw std::length_error("Human accessory catalogue size");
    std::vector<AccessoryEntry> result;result.reserve(components.size());std::set<std::string> ids,names;
    for(const auto &value:components)
    {
        AccessoryEntry entry;entry.info={as<std::string>(field(value,"id")),as<std::string>(field(value,"name")),as<std::string>(field(value,"category"))};
        object3d_detail::validateName(entry.info.id);object3d_detail::validateName(entry.info.name);object3d_detail::validateName(entry.info.category);
        if(!ids.insert(entry.info.id).second||!names.insert(entry.info.name).second)throw std::invalid_argument("Duplicate human accessory ID or name");
        entry.proxy=as<std::string>(field(value,"proxy"));entry.roughness=numeric(field(value,"roughness"));
        if(entry.proxy.empty()||entry.proxy.is_absolute()||!std::isfinite(entry.roughness)||entry.roughness<0||entry.roughness>1)throw std::invalid_argument("Invalid human accessory catalogue entry");
        for(const auto &part:entry.proxy)if(part=="..")throw std::invalid_argument("Human accessory catalogue path escapes directory");
        result.push_back(std::move(entry));
    }
    std::sort(result.begin(),result.end(),[](const auto &a,const auto &b){return a.info.id<b.info.id;});return result;
}
std::vector<AccessoryEntry> selectAccessories(const std::vector<AccessoryEntry> &entries,const HumanAccessorySelection &selection)
{
    if(selection.componentIds.empty()||selection.componentIds.size()>32)throw std::invalid_argument("Invalid human accessory selection size");
    std::set<std::string> selected;std::vector<AccessoryEntry> result;result.reserve(selection.componentIds.size());
    for(const auto &id:selection.componentIds)
    {
        if(!selected.insert(id).second)throw std::invalid_argument("Duplicate human accessory selection");
        const auto found=std::lower_bound(entries.begin(),entries.end(),id,[](const auto &entry,const auto &value){return entry.info.id<value;});
        if(found==entries.end()||found->info.id!=id)throw std::invalid_argument("Unknown human accessory selection");
        result.push_back(*found);
    }
    return result;
}
} // namespace iiActorObject::NSCharacter::human_detail
