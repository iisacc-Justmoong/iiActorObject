#pragma once
#include "../third_party/ufbx/ufbx.h"
#include <cstddef>
#include <string>
#include <vector>

namespace iiActorObject::object3d_detail
{
// Channel pointers borrow a parsed FBX scene until animation planning finishes.
struct FbxImportedMorph final
{
    std::string name;
    std::size_t mesh,target;
    std::vector<const ufbx_blend_channel *> channels;
};
using FbxImportedMorphs=std::vector<FbxImportedMorph>;
struct FbxImportedProgressiveControl final
{
    std::string name;
    std::vector<const ufbx_blend_channel *> channels;
};
}
