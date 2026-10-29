#pragma once
#include <iiActorObject/Transform.h>
#include <cstdint>
#include <string_view>

namespace iiActorObject::object3d_detail
{
// Names borrow the originating bone storage through the enclosing export.
struct FbxSkinBone final
{
    std::string_view name;
    std::int64_t nodeId;
    Matrix4 inverseBind,bindWorld;
};
}
