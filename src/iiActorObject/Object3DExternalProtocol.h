#pragma once
// Private FBX property encoding shared by the core writer and reader.
#include "Object3DExternalEdits.h"
#include <charconv>
#include <iomanip>
#include <locale>
#include <map>
#include <set>
#include <sstream>

namespace iiActorObject::object3d_detail
{
inline std::string externalIds(std::span<const std::uint32_t> ids)
{std::string text;for(auto id:ids){if(!text.empty())text.push_back(',');text+=std::to_string(id);}return text;}
inline std::vector<std::uint32_t> externalIds(std::string_view text,std::size_t count)
{
    std::vector<std::uint32_t> ids;ids.reserve(count);std::size_t offset=0;
    while(offset<text.size())
    {
        if(ids.size()==count)throw std::invalid_argument("External identity count exceeds source dimensions");
        const auto end=text.find(',',offset),limit=end==std::string_view::npos?text.size():end;std::uint32_t value=0;
        const auto result=std::from_chars(text.data()+offset,text.data()+limit,value);if(result.ec!=std::errc{}||result.ptr!=text.data()+limit||value==NoTopologyIndex)throw std::invalid_argument("Malformed external source ID");ids.push_back(value);
        offset=limit+(end==std::string_view::npos?0:1);if(end!=std::string_view::npos&&offset==text.size())throw std::invalid_argument("Trailing external ID delimiter");
    }
    if(ids.size()!=count)throw std::invalid_argument("External source identity dimensions");return ids;
}
inline std::string externalMorphs(std::span<const std::pair<std::string,std::string>> names)
{std::ostringstream out;out.imbue(std::locale::classic());out<<"iiActorObjectMorphNames 1 "<<names.size()<<'\n';for(const auto &[foreign,source]:names)out<<std::quoted(foreign)<<' '<<std::quoted(source)<<'\n';return out.str();}
inline std::map<std::string,std::string> externalMorphs(std::string_view text,std::size_t maximum)
{
    std::istringstream in{std::string(text)};in.imbue(std::locale::classic());std::string magic;unsigned version=0;std::size_t count=0;if(!(in>>magic>>version>>count)||magic!="iiActorObjectMorphNames"||version!=1||count>maximum)throw std::invalid_argument("External morph identity header");
    std::map<std::string,std::string> names;std::set<std::string> sources;
    for(std::size_t i=0;i<count;++i){std::string a,b;if(!(in>>std::quoted(a)>>std::quoted(b))||a.empty()||b.empty()||a.size()>4096||b.size()>4096||a.find('\0')!=std::string::npos||b.find('\0')!=std::string::npos||!sources.insert(b).second||!names.emplace(a,b).second)throw std::invalid_argument("Malformed external morph identity");}
    in>>std::ws;if(!in.eof())throw std::invalid_argument("Trailing external morph identities");return names;
}
} // namespace iiActorObject::object3d_detail
