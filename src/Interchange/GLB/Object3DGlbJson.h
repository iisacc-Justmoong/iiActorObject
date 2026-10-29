#pragma once
#include <iiJson/Json.h>
#include <cstddef>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace iiActorObject { struct GlbImportOptions; }
namespace iiActorObject::object3d_detail::glb_import
{
using Value=ii::json::Value;
using Array=Value::Array;
// Checked GLB JSON values and the common unsupported-feature policy.
[[noreturn]] void fail(std::string_view message);
const Value::Object &object(const Value &value);
const Array &array(const Value &value);
const Value *field(const Value &value,std::string_view name);
const Value *optionalObjectField(const Value *value,std::string_view name);
const Value &required(const Value &value,std::string_view name);
const Array &list(const Value &value,std::string_view name);
std::string_view text(const Value &value);
std::size_t integer(const Value &value);
std::size_t optionalInteger(const Value &value,std::string_view name,std::size_t fallback=0);
double number(const Value &value);
bool boolean(const Value &value);
std::vector<double> numbers(const Value &value,std::size_t count);
std::string name(const Value &value,std::string fallback,std::set<std::string> &used,
                 std::vector<std::string> *warnings=nullptr);
void unsupported(const GlbImportOptions &options,std::vector<std::string> &warnings,std::string message);
void extensions(const Value &value,const GlbImportOptions &options,std::vector<std::string> &warnings);
} // namespace iiActorObject::object3d_detail::glb_import
