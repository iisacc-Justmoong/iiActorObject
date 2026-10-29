#include "Object3DFbxSharedGeometry.h"

namespace iiActorObject::object3d_detail
{
namespace
{
constexpr std::size_t Limit=128'000'000;
bool sameNode(const fbx_binary::Node &a,const fbx_binary::Node &b,std::size_t &work)
{
    if(a.name!=b.name||a.properties.size()!=b.properties.size()||a.children.size()!=b.children.size())return false;
    for(std::size_t i=0;i<a.properties.size();++i)
    {
        const auto &left=a.properties[i].bytes,&right=b.properties[i].bytes;
        if(left.size()!=right.size())return false;
        if(work>Limit||left.size()>Limit-work)throw std::length_error("Progressive FBX shared geometry comparison budget");
        work+=left.size();if(left!=right)return false;
    }
    for(std::size_t i=0;i<a.children.size();++i)if(!sameNode(a.children[i],b.children[i],work))return false;
    return true;
}
void account(std::size_t count,std::size_t &work)
{
    if(work>Limit||count>Limit-work)throw std::length_error("Progressive FBX shared mesh comparison budget");
    work+=count;
}
}
void validateFbxSharedGeometry(const fbx_binary::Node &candidate,const fbx_binary::Node &leader,
    const Object3DMesh &mesh,const Object3DMesh &leaderMesh,std::size_t &work)
{
    if(candidate.children.size()!=leader.children.size())
        throw SharedProgressiveGeometryMismatch("Shared progressive FBX meshes have different geometry");
    for(std::size_t i=0;i<candidate.children.size();++i)
        if(!sameNode(candidate.children[i],leader.children[i],work))
            throw SharedProgressiveGeometryMismatch("Shared progressive FBX meshes have different geometry");
    const auto *source=mesh.topology?&*mesh.topology:nullptr;
    const auto *firstSource=leaderMesh.topology?&*leaderMesh.topology:nullptr;
    const auto count=source?source->points.size():mesh.geometry.vertices.size();
    if(bool(source)!=bool(firstSource)||count!=(firstSource?firstSource->points.size():leaderMesh.geometry.vertices.size())||
       (source?source->skinning:mesh.geometry.skinning)!=(firstSource?firstSource->skinning:leaderMesh.geometry.skinning))
        throw SharedProgressiveGeometryMismatch("Shared progressive FBX meshes have different skinning");
    account(count,work);
    for(std::size_t i=0;i<count;++i)
    {
        const auto &a=source?source->points[i].weights:mesh.geometry.vertices[i].weights;
        const auto &b=firstSource?firstSource->points[i].weights:leaderMesh.geometry.vertices[i].weights;
        if(a.size()!=b.size())throw SharedProgressiveGeometryMismatch("Shared progressive FBX meshes have different skin weights");
        account(a.size(),work);
        for(std::size_t w=0;w<a.size();++w)if(a[w].bone!=b[w].bone||a[w].weight!=b[w].weight)
            throw SharedProgressiveGeometryMismatch("Shared progressive FBX meshes have different skin weights");
    }
}
void validateFbxSharedMorphs(const std::vector<TopologyMorph> &morphs,const std::vector<TopologyMorph> &leaderMorphs,
    const std::vector<MorphValue> &defaults,const std::vector<MorphValue> &leaderDefaults,
    const std::set<std::string,std::less<>> &consumedTargets,std::size_t &work)
{
    account(morphs.size(),work);
    for(const auto &m:morphs)
    {
        account(m.deltas.size(),work);if(m.normalDeltas)account(m.normalDeltas->size(),work);
        if(!consumedTargets.contains(m.name))
            throw SharedProgressiveGeometryMismatch("Shared progressive FBX geometry cannot preserve independent ordinary morph controls");
    }
    if(morphs!=leaderMorphs||defaults!=leaderDefaults)
        throw SharedProgressiveGeometryMismatch("Shared progressive FBX meshes have different morph shapes or defaults");
}
}
