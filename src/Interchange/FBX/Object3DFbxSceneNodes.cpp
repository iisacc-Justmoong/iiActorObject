#include "Object3DFbxSceneNodes.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace iiActorObject::object3d_detail::fbx_scene_nodes
{
using namespace fbx_binary;
Node pInt(std::string_view n, std::int32_t v)
{
    return {"P", {prop(n), prop("int"), prop("Integer"), prop(""), prop(v)}, {}};
}
Node pNumber(std::string_view n, double v)
{
    return {"P", {prop(n), prop("double"), prop("Number"), prop(""), prop(v)}, {}};
}
Node pEnum(std::string_view name,std::int32_t value)
{return {"P",{prop(name),prop("enum"),prop(""),prop(""),prop(value)}, {}};}
Node pVector(std::string_view n, std::string_view type, Vector3 v)
{
    return {"P", {prop(n), prop(type), prop(""), prop("A"), prop(v.x), prop(v.y), prop(v.z)}, {}};
}
std::string objectName(std::string_view name, std::string_view type)
{
    std::string out(name);
    out.append("\0\1", 2);
    out += type;
    return out;
}
Vector3 euler(Quaternion q)
{
    q = q.normalized();
    const auto degrees = 180 / std::numbers::pi;
    return {degrees * std::atan2(2 * (q.w * q.x + q.y * q.z), 1 - 2 * (q.x * q.x + q.y * q.y)),
            degrees * std::asin(std::clamp(2 * (q.w * q.y - q.z * q.x), -1.0, 1.0)),
            degrees * std::atan2(2 * (q.w * q.z + q.x * q.y), 1 - 2 * (q.y * q.y + q.z * q.z))};
}
Node model(std::int64_t id, std::string_view name, std::string_view type, Transform t)
{
    return {"Model",
            {prop(id), prop(objectName(name, "Model")), prop(type)},
            {value("Version", prop(232)),
             {"Properties70",
              {},
              {pVector("Lcl Translation", "Lcl Translation", t.position),
               pVector("Lcl Rotation", "Lcl Rotation", euler(t.rotation)),
               pVector("Lcl Scaling", "Lcl Scaling", t.scale), pInt("RotationOrder", 0),
               pInt("InheritType", 1)}},
             value("Culling", prop("CullingOff"))}};
}
Node externalText(std::string_view name,std::string_view text)
{return {"P",{prop(name),prop("KString"),prop(""),prop("U"),prop(text)}, {}};}
Node externalInteger(std::string_view name,std::int32_t number)
{return {"P",{prop(name),prop("int"),prop("Integer"),prop("U"),prop(number)}, {}};}
Node externalNumber(std::string_view name,double number)
{return {"P",{prop(name),prop("double"),prop("Number"),prop("U"),prop(number)}, {}};}
std::int32_t externalIndex(std::size_t index)
{if(index>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))throw std::length_error("FBX external source index range");return static_cast<std::int32_t>(index);}
Node layerElement(std::string_view type,std::int32_t index)
{
    return {"LayerElement", {}, {value("Type", prop(type)), value("TypedIndex", prop(index))}};
}
} // namespace iiActorObject::object3d_detail::fbx_scene_nodes
