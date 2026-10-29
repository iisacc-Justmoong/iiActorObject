#include "Object3DGlbTangentNotes.h"
#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

namespace iiActorObject::object3d_detail::glb_import
{
void appendGlbFbxTangentNotes(const Value& primitive,std::size_t &noteCount,bool &truncated,std::vector<std::string>& warnings)
{
    const auto *value=optionalObjectField(field(primitive,"extras"),"iiActorObjectFbxTangentNotes");
    if(!value)return;
    const auto record=[&](std::string message)
    {
        if(noteCount<32){warnings.push_back(std::move(message));++noteCount;}
        else if(!truncated)
        {warnings.emplace_back("additional GLB FBX tangent notes were ignored");truncated=true;}
    };
    const auto entries=value->get<Array>();
    if(!entries){record("unrecognized GLB FBX tangent notes were ignored");return;}
    constexpr std::array known{
        std::string_view{"source_fbx_tangents_stale"},std::string_view{"source_fbx_uv_layer_missing"},
        std::string_view{"source_fbx_tangent_missing"},std::string_view{"source_fbx_uv_ambiguous"},
        std::string_view{"source_fbx_unbound_uv"},std::string_view{"source_fbx_other_uv_layers_omitted"},
        std::string_view{"source_fbx_corner_mapping"},std::string_view{"source_fbx_morph_work_budget"},
        std::string_view{"source_fbx_handedness_unknown"},std::string_view{"source_fbx_binormal_w_omitted"},
        std::string_view{"source_fbx_morph_tangents_unavailable"},std::string_view{"source_fbx_binormal_approximated"},
        std::string_view{"source_fbx_tangent_w_conflict"},std::string_view{"source_fbx_triangle_handedness"},
        std::string_view{"corner_budget"},std::string_view{"polygon_mapping"},std::string_view{"polygon_tangent_conflict"}};
    for(const auto &entry:entries->get())
    {
        if(noteCount>=32)
        {record({});break;}
        const auto str=entry.get<std::string>();
        if(!str||str->get().size()>64||std::find(known.begin(),known.end(),str->get())==known.end())
            record("unrecognized GLB FBX tangent note was ignored");
        else record("GLB FBX tangent note: "+str->get());
    }
}


}
