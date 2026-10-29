#pragma once
#include "Object3DGlbJson.h"
#include <cstddef>
#include <string>
#include <vector>

namespace iiActorObject::object3d_detail::glb_import
{
// Records bounded diagnostics from the optional FBX tangent note metadata.
void appendGlbFbxTangentNotes(const Value &primitive,std::size_t &noteCount,
    bool &truncated,std::vector<std::string> &warnings);
}
