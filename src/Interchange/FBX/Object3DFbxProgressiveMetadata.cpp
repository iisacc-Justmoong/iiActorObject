#include "Object3DFbxProgressiveMetadata.h"
#include <algorithm>
#include <stdexcept>

namespace iiActorObject::object3d_detail
{
FbxProgressiveMetadata readFbxProgressiveMetadata(const ufbx_blend_channel &channel,bool restore,
    bool progressiveAvailable,std::size_t maximumMorphs)
{
    using namespace fbx_metadata;
    FbxProgressiveMetadata result;const auto *dom=channel.element.dom_node;
    const auto *marker=child(dom,"iiActorObjectProgressive");
    if(marker&&number(marker)!=1)throw std::invalid_argument("Invalid FBX progressive marker");
    result.marker=marker!=nullptr;
    const auto get=[&](const char *name){return restore?uniqueChild(dom,name):child(dom,name);};
    const auto *group=get("iiActorObjectProgressiveGroup"),*groupSize=get("iiActorObjectProgressiveGroupSize");
    const auto *sparse=get("iiActorObjectProgressiveSparse"),*presence=get("iiActorObjectProgressivePresence");
    result.grouped=restore&&(group||groupSize);result.sparse=restore&&(sparse||presence);
    result.ignoredGroup=!restore&&(group||groupSize);result.ignoredSparse=!restore&&(sparse||presence);
    if(result.grouped)
    {
        if(!group||!groupSize||!marker)throw std::invalid_argument("Incomplete FBX progressive control group metadata");
        result.groupId=number(group);result.groupSize=number(groupSize);
        if(result.groupId==0||result.groupId>65535||result.groupSize<2||result.groupSize>65535)
            throw std::invalid_argument("Invalid FBX progressive control group metadata");
        if(!progressiveAvailable)throw std::invalid_argument("FBX shared progressive controls require the progressive import path");
    }
    if(result.sparse)
    {
        if(!result.grouped||!sparse||!presence||number(sparse)!=1)
            throw std::invalid_argument("Incomplete FBX progressive sparse-key metadata");
        result.presence=array(presence,maximumMorphs);
        if(result.presence.size()!=channel.keyframes.count||
           std::any_of(result.presence.begin(),result.presence.end(),[](auto value){return value>1;})||
           std::none_of(result.presence.begin(),result.presence.end(),[](auto value){return value==1;}))
            throw std::invalid_argument("Invalid FBX progressive sparse-key presence");
    }
    return result;
}
} // namespace iiActorObject::object3d_detail
