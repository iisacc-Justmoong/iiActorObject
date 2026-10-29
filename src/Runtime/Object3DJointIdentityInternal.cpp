#include "Object3DJointIdentityInternal.h"
#include <map>
#include <stdexcept>

namespace iiActorObject
{
namespace object3d_detail
{
void validateEditedJointIdentity(const Object3DAsset &previous,const Object3DAsset &next,EditedRigJointIdentity policy)
{
    const auto &a=previous.data(),&b=next.data();
    if(previous.hasRig()!=next.hasRig()||a.bones.size()!=b.bones.size())
        throw std::invalid_argument("Edited rig skeleton size or kind changed");
    if(policy==EditedRigJointIdentity::TrustStableIndices)return;
    std::map<std::string_view,BoneIndex,std::less<>> semantics;
    for(const auto &item:b.semantics)semantics.emplace(item.name,item.bone);
    if(a.semantics.size()!=b.semantics.size())throw std::invalid_argument("Edited rig joint semantics changed");
    std::vector<bool> anchored(a.bones.size(),false);
    for(const auto &item:a.semantics)
    {
        const auto match=semantics.find(item.name);
        if(match==semantics.end()||match->second!=item.bone)
            throw std::invalid_argument("Edited rig joint semantics changed");
        anchored.at(item.bone)=true;
    }
    for(std::size_t i=0;i<a.bones.size();++i)
        if(a.bones[i].name!=b.bones[i].name&&!anchored[i])
            throw std::invalid_argument("Edited rig joint identity needs a stable semantic or explicit index trust");
}
}
} // namespace iiActorObject
