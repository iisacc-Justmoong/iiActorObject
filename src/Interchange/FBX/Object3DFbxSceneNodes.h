#pragma once
#include "Object3DFbxBinary.h"
#include "Transform.h"

namespace iiActorObject::object3d_detail::fbx_scene_nodes
{
// Build common FBX property, object, model and layer nodes without owning a scene.
fbx_binary::Node pInt(std::string_view name,std::int32_t value);
fbx_binary::Node pNumber(std::string_view name,double value);
fbx_binary::Node pEnum(std::string_view name,std::int32_t value);
fbx_binary::Node pVector(std::string_view name,std::string_view type,Vector3 value);
std::string objectName(std::string_view name,std::string_view type);
fbx_binary::Node model(std::int64_t id,std::string_view name,std::string_view type,Transform transform = {});
fbx_binary::Node externalText(std::string_view name,std::string_view value);
fbx_binary::Node externalInteger(std::string_view name,std::int32_t value);
fbx_binary::Node externalNumber(std::string_view name,double value);
std::int32_t externalIndex(std::size_t index);
fbx_binary::Node layerElement(std::string_view type,std::int32_t index = 0);
} // namespace iiActorObject::object3d_detail::fbx_scene_nodes
