#include "Object3DFbxGeometryRegistry.h"
#include "Object3DFbxSharedGeometry.h"
#include "Object3DMesh.h"
#include <stdexcept>
#include <utility>
namespace iiActorObject::object3d_detail
{
FbxGeometryRegistry::FbxGeometryRegistry(std::size_t count):entries_(count){}
std::int64_t FbxGeometryRegistry::stage(std::size_t meshIndex,std::size_t leaderIndex,std::int64_t geometryId,
    fbx_binary::Node candidate,std::span<const Object3DMesh> meshes,fbx_binary::Node &objects,std::size_t &work)
{
    if(meshes.size()!=entries_.size()||meshIndex>=entries_.size()||leaderIndex>=entries_.size())
        throw std::invalid_argument("Invalid FBX geometry registry index");
    if(objects_&&objects_!=&objects)throw std::invalid_argument("FBX geometry registry node changed");
    if(meshIndex==leaderIndex)
    {
        if(entries_[meshIndex])throw std::invalid_argument("Duplicate FBX geometry registration");
        const auto nodeIndex=objects.children.size();
        objects.children.push_back(std::move(candidate));
        entries_[meshIndex]=Entry{nodeIndex,geometryId};objects_=&objects;
        return geometryId;
    }
    const auto &leader=entries_[leaderIndex];
    if(!leader)throw SharedProgressiveGeometryMismatch("Shared progressive FBX leader was not emitted");
    if(leader->nodeIndex>=objects.children.size())throw std::invalid_argument("FBX geometry registry node removed");
    validateFbxSharedGeometry(candidate,objects.children[leader->nodeIndex],meshes[meshIndex],meshes[leaderIndex],work);
    return leader->geometryId;
}
}
