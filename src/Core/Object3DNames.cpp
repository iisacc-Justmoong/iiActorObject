#include "Object3DNames.h"
#include <stdexcept>

namespace iiActorObject::object3d_detail
{
void validateName(std::string_view s)
{
    if(s.empty()||s.size()>1024||s.find('\0')!=std::string_view::npos)
        throw std::invalid_argument("Character names must be nonempty, bounded and contain no NUL");
    for(std::size_t i=0;i<s.size();)
    {
        const auto first=static_cast<unsigned char>(s[i++]);if(first<128)continue;
        unsigned count=0,code=0,minimum=0;
        if(first>=0xc2&&first<=0xdf){count=1;code=first&31;minimum=0x80;}
        else if(first>=0xe0&&first<=0xef){count=2;code=first&15;minimum=0x800;}
        else if(first>=0xf0&&first<=0xf4){count=3;code=first&7;minimum=0x10000;}
        else throw std::invalid_argument("Character names require UTF-8");
        if(count>s.size()-i)throw std::invalid_argument("Truncated UTF-8 name");
        while(count--){const auto b=static_cast<unsigned char>(s[i++]);if((b&0xc0)!=0x80)throw std::invalid_argument("Invalid UTF-8 continuation");code=(code<<6)|(b&63);}
        if(code<minimum||code>0x10ffff||(code>=0xd800&&code<=0xdfff))throw std::invalid_argument("Invalid UTF-8 code point");
    }
}
}
