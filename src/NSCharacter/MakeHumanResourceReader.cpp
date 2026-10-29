#include "MakeHumanResourceReader.h"
#include <iiActorObject/MeshTopologyCore.h>
#include <fstream>
#include <utility>
#include <variant>

namespace iiActorObject::NSCharacter::human_detail
{
const Value &field(const Value &value,std::string_view key)
{
    const auto found=value.find(key);
    if(!found)throw std::invalid_argument("Missing template field: "+std::string(key));
    return found->get();
}
double numeric(const Value &value)
{
    if(const auto p=std::get_if<double>(&value.storage()))return *p;
    if(const auto p=std::get_if<std::uint64_t>(&value.storage()))return static_cast<double>(*p);
    if(const auto p=std::get_if<std::int64_t>(&value.storage()))return static_cast<double>(*p);
    throw std::invalid_argument("Expected numeric template value");
}
std::uint32_t pointId(const Value &value)
{
    if(const auto p=std::get_if<std::uint64_t>(&value.storage());p&&*p<NoTopologyIndex)return static_cast<std::uint32_t>(*p);
    if(const auto p=std::get_if<std::int64_t>(&value.storage());p&&*p>=0&&*p<NoTopologyIndex)return static_cast<std::uint32_t>(*p);
    throw std::invalid_argument("Expected nonnegative source point ID");
}
Files::Files(std::filesystem::path directory,std::size_t limit)
    :root_(std::filesystem::canonical(directory)),left_(limit){}
std::string Files::read(const std::filesystem::path &name)
{
    if(name.empty()||name.is_absolute())throw std::invalid_argument("Invalid template relative path");
    for(const auto &part:name)if(part=="..")throw std::invalid_argument("Template path escapes directory");
    const auto file=std::filesystem::canonical(root_/name);
    const auto relative=file.lexically_relative(root_);
    if(relative.empty()||*relative.begin()=="..")throw std::invalid_argument("Template link escapes directory");
    const auto bytes=std::filesystem::file_size(file);
    if(bytes>left_)throw std::length_error("Template total byte limit exceeded");
    std::ifstream input(file,std::ios::binary);if(!input)throw std::runtime_error("Cannot open template data");
    std::string data(static_cast<std::size_t>(bytes),'\0');
    if(!input.read(data.data(),static_cast<std::streamsize>(data.size())))throw std::runtime_error("Cannot read complete template data");
    left_-=data.size();return data;
}
Value Files::json(const std::filesystem::path &name)
{
    auto data=read(name);ii::json::Limits limits;limits.input_bytes=64*1024*1024;limits.items=2'000'000;
    auto parsed=ii::json::parse(data,limits);
    if(!parsed)throw std::invalid_argument("Invalid template JSON: "+parsed.error().message);
    return std::move(*parsed);
}
std::size_t Files::remaining() const noexcept{return left_;}
} // namespace iiActorObject::NSCharacter::human_detail
