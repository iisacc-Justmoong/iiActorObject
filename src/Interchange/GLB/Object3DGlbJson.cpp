#include "Object3DGlbJson.h"
#include "Object3DGlbImportOptions.h"
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail::glb_import
{
[[noreturn]] void fail(std::string_view message)
{throw std::invalid_argument("GLB import: "+std::string(message));}
const Value::Object& object(const Value& value)
{const auto result=value.get<Value::Object>();if(!result)fail("expected an object");return result->get();}
const Array& array(const Value& value)
{const auto result=value.get<Array>();if(!result)fail("expected an array");return result->get();}
const Value* field(const Value& value,std::string_view name)
{const auto& values=object(value);const auto i=values.find(name);return i==values.end()?nullptr:&i->second;}
const Value* optionalObjectField(const Value* value,std::string_view name)
{return value&&value->get<Value::Object>()?field(*value,name):nullptr;}
const Value& required(const Value& value,std::string_view name)
{const auto* result=field(value,name);if(!result)fail("missing "+std::string(name));return *result;}
const Array& list(const Value& value,std::string_view name)
{static const Array empty;const auto* v=field(value,name);return v?array(*v):empty;}
std::string_view text(const Value& value)
{const auto result=value.get<std::string>();if(!result)fail("expected a string");return result->get();}
std::size_t integer(const Value& value)
{
    if(const auto* n=std::get_if<std::uint64_t>(&value.storage()))
    {if(*n>std::numeric_limits<std::size_t>::max())fail("integer out of range");return static_cast<std::size_t>(*n);}
    if(const auto* n=std::get_if<std::int64_t>(&value.storage());n&&*n>=0)
    {if(static_cast<std::uint64_t>(*n)>std::numeric_limits<std::size_t>::max())fail("integer out of range");return static_cast<std::size_t>(*n);}
    fail("expected a nonnegative integer");
}
std::size_t optionalInteger(const Value& value,std::string_view name,std::size_t fallback)
{const auto* v=field(value,name);return v?integer(*v):fallback;}
double number(const Value& value)
{
    if(const auto* n=std::get_if<double>(&value.storage()))return *n;
    if(const auto* n=std::get_if<std::uint64_t>(&value.storage()))return static_cast<double>(*n);
    if(const auto* n=std::get_if<std::int64_t>(&value.storage()))return static_cast<double>(*n);
    fail("expected a number");
}
bool boolean(const Value& value)
{const auto result=value.get<bool>();if(!result)fail("expected a boolean");return result->get();}
std::vector<double> numbers(const Value& value,std::size_t count)
{const auto& values=array(value);if(values.size()!=count)fail("unexpected numeric array length");std::vector<double> result;result.reserve(count);for(const auto& v:values){auto n=number(v);if(!std::isfinite(n))fail("nonfinite numeric value");result.push_back(n);}return result;}
std::string name(const Value& value,std::string fallback,std::set<std::string>& used,std::vector<std::string>* warnings)
{
    std::string base=field(value,"name")?std::string(text(*field(value,"name"))):std::move(fallback);
    if(base.empty())base="Unnamed";
    if(base.size()>900||base.find('\0')!=std::string::npos)fail("invalid source name");
    auto result=base;std::size_t suffix=1;
    while(!used.insert(result).second)result=base+"_"+std::to_string(suffix++);
    if(result!=base&&warnings)warnings->emplace_back("Duplicate GLB name was made unique: "+base);
    return result;
}
void unsupported(const GlbImportOptions& options,std::vector<std::string>& warnings,std::string message)
{if(!options.allowDiscardUnsupportedAttributes)fail(message);if(warnings.size()>=4096)fail("diagnostic count limit");warnings.push_back(std::move(message));}
void extensions(const Value& value,const GlbImportOptions& options,std::vector<std::string>& warnings)
{if(const auto* v=field(value,"extensions");v&&!object(*v).empty())unsupported(options,warnings,"Unsupported GLB extension data discarded");}
} // namespace iiActorObject::object3d_detail::glb_import
