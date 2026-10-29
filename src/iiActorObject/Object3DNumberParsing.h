#pragma once
#include <charconv>
#include <cmath>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

namespace iiActorObject::object3d_detail
{
// A strict decimal grammar shared with the classic-locale fallback. Leading
// plus signs are deliberately rejected, matching floating from_chars.
inline bool decimalSyntax(std::string_view text)
{
    std::size_t i=0;if(i<text.size()&&text[i]=='-')++i;
    bool digit=false;while(i<text.size()&&text[i]>='0'&&text[i]<='9'){digit=true;++i;}
    if(i<text.size()&&text[i]=='.')
    {++i;while(i<text.size()&&text[i]>='0'&&text[i]<='9'){digit=true;++i;}}
    if(!digit)return false;
    if(i<text.size()&&(text[i]=='e'||text[i]=='E'))
    {
        ++i;if(i<text.size()&&(text[i]=='+'||text[i]=='-'))++i;
        const auto begin=i;while(i<text.size()&&text[i]>='0'&&text[i]<='9')++i;
        if(begin==i)return false;
    }
    return i==text.size();
}
template<class T> bool readClassicReal(std::string_view text,T &value)
{
    static_assert(std::is_floating_point_v<T>);
    if(!decimalSyntax(text))return false;
    std::istringstream input{std::string(text)};input.imbue(std::locale::classic());
    T parsed{};input>>std::noskipws>>parsed;
    if(!input.eof()||!std::isfinite(parsed))return false;
    // Some implementations set failbit while returning a representable subnormal.
    if(input.fail()&&std::abs(parsed)>=std::numeric_limits<T>::min())return false;
    if(parsed==0)
        for(const auto c:text){if(c=='e'||c=='E')break;if(c>='1'&&c<='9')return false;}
    value=parsed;return true;
}
template<class T> bool readNumber(std::string_view text,T &value)
{
    if(text.empty())return false;
    if constexpr(std::is_floating_point_v<T>)
    {
        if constexpr(requires(const char *a,const char *b,T &v){std::from_chars(a,b,v,std::chars_format::general);})
        {
            const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value,std::chars_format::general);
            return parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()&&std::isfinite(value);
        }
        else return readClassicReal(text,value);
    }
    else
    {
        const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
        return parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size();
    }
}
} // namespace iiActorObject::object3d_detail
