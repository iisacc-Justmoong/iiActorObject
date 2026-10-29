#include "Object3DImage.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace iiActorObject
{
struct Object3DImage::Data final
{
    std::string name,mime;
    std::vector<std::uint8_t> encoded;
    std::uint32_t width=0,height=0;
};
namespace
{
std::uint32_t big(std::span<const std::uint8_t> bytes,std::size_t offset)
{
    if(offset>bytes.size()||bytes.size()-offset<4)throw std::invalid_argument("Truncated image number");
    return std::uint32_t(bytes[offset])<<24|std::uint32_t(bytes[offset+1])<<16|std::uint32_t(bytes[offset+2])<<8|bytes[offset+3];
}
std::uint32_t crc(std::span<const std::uint8_t> bytes)
{
    std::uint32_t value=0xffffffff;for(auto byte:bytes){value^=byte;for(unsigned bit=0;bit<8;++bit)value=(value>>1)^(0xedb88320u&-(value&1u));}return ~value;
}
std::array<std::uint32_t,2> png(std::span<const std::uint8_t> bytes)
{
    constexpr std::array<std::uint8_t,8> signature{137,80,78,71,13,10,26,10};
    if(bytes.size()<45||!std::equal(signature.begin(),signature.end(),bytes.begin()))throw std::invalid_argument("Expected a complete PNG resource");
    std::array<std::uint32_t,2> dimensions{};bool header=false,pixels=false,end=false;std::size_t offset=8;
    while(offset<bytes.size())
    {
        if(bytes.size()-offset<12)throw std::invalid_argument("Truncated PNG chunk");
        const auto length=big(bytes,offset);if(length>bytes.size()-offset-12)throw std::invalid_argument("PNG chunk size exceeds resource");
        const std::string_view type(reinterpret_cast<const char*>(bytes.data()+offset+4),4);
        if(crc(bytes.subspan(offset+4,length+4))!=big(bytes,offset+8+length))throw std::invalid_argument("PNG chunk checksum mismatch");
        if(!header&&type!="IHDR")throw std::invalid_argument("PNG must begin with IHDR");
        if(type=="IHDR")
        {
            if(header||length!=13)throw std::invalid_argument("Invalid PNG IHDR");header=true;dimensions={big(bytes,offset+8),big(bytes,offset+12)};
            const auto depth=bytes[offset+16],color=bytes[offset+17];
            const bool valid=(color==0&&(depth==1||depth==2||depth==4||depth==8||depth==16))||(color==2&&(depth==8||depth==16))||
                (color==3&&(depth==1||depth==2||depth==4||depth==8))||((color==4||color==6)&&(depth==8||depth==16));
            if(!dimensions[0]||!dimensions[1]||dimensions[0]>16384||dimensions[1]>16384||!valid||bytes[offset+18]||bytes[offset+19]||bytes[offset+20]>1)throw std::invalid_argument("Unsupported PNG dimensions or coding");
        }
        else if(type=="IDAT")pixels=true;
        else if(type=="IEND"){if(length||!pixels||offset+12!=bytes.size())throw std::invalid_argument("Invalid PNG end");end=true;}
        offset+=length+12;
    }
    if(!header||!pixels||!end)throw std::invalid_argument("Incomplete PNG resource");return dimensions;
}
}
Object3DImage::Object3DImage(std::string name,std::vector<std::uint8_t> encoded,std::string mimeType)
{
    if(name.empty()||name.size()>1024||name.find('\0')!=std::string::npos||mimeType!="image/png")throw std::invalid_argument("Invalid image name or MIME");
    if(encoded.size()>64*1024*1024)throw std::length_error("Encoded image byte budget");const auto size=png(encoded);
    data_=std::make_shared<const Data>(Data{std::move(name),std::move(mimeType),std::move(encoded),size[0],size[1]});
}
bool Object3DImage::empty() const noexcept{return !data_;}
std::string_view Object3DImage::name() const noexcept{return data_?data_->name:std::string_view{};}
std::string_view Object3DImage::mimeType() const noexcept{return data_?data_->mime:std::string_view{};}
std::span<const std::uint8_t> Object3DImage::encoded() const noexcept{return data_?std::span<const std::uint8_t>(data_->encoded):std::span<const std::uint8_t>{};}
std::uint32_t Object3DImage::width() const noexcept{return data_?data_->width:0;}
std::uint32_t Object3DImage::height() const noexcept{return data_?data_->height:0;}
bool Object3DImage::operator==(const Object3DImage &other) const noexcept
{return data_==other.data_||(data_&&other.data_&&data_->name==other.data_->name&&data_->mime==other.data_->mime&&data_->encoded==other.data_->encoded);}
} // namespace iiActorObject
