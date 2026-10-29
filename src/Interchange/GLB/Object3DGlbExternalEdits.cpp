#include "Object3DInterchange.h"
#include "Object3DNames.h"
#include <algorithm>
#include <array>
#include <map>
#include <numeric>
#include <stdexcept>

namespace iiActorObject
{
namespace
{
using Triangle=std::array<std::uint32_t,3>;
struct SourceTriangle final
{
    std::uint32_t id=0;
    Triangle points{},layouts{};
};
Triangle sorted(Triangle value)
{
    std::sort(value.begin(),value.end());
    if(value[0]==value[1]||value[1]==value[2])
        throw std::invalid_argument("GLB external triangle repeats a source point");
    return value;
}
bool sameWinding(const Triangle &source,const Triangle &foreign)
{
    for(std::size_t offset=0;offset<3;++offset)
        if(source[0]==foreign[offset]&&source[1]==foreign[(offset+1)%3]&&source[2]==foreign[(offset+2)%3])return true;
    return false;
}
void checkBones(const Object3DAsset &source,const Object3DAsset &snapshot,const GlbExternalIdentity &identity)
{
    if(!source.hasRig())
    {
        if(!identity.bones.empty()||identity.syntheticRoot||snapshot.hasRig())throw std::invalid_argument("GLB external static rig changed");
        return;
    }
    const auto sourceCount=source.data().bones.size(),foreignCount=snapshot.data().bones.size();
    if(!snapshot.hasRig()||identity.bones.size()!=sourceCount)
        throw std::invalid_argument("GLB external joint identities are incomplete");
    std::vector<bool> sourceSeen(sourceCount),foreignSeen(foreignCount);
    if(identity.syntheticRoot)
    {
        const auto root=*identity.syntheticRoot;
        if(foreignCount<=sourceCount||foreignCount-sourceCount!=1||root>=foreignCount||
           snapshot.data().bones[root].parent!=NoBone||snapshot.data().bones[root].bindTransform!=Transform{})
            throw std::invalid_argument("GLB external synthetic root is invalid");
        foreignSeen[root]=true;
    }
    else if(foreignCount!=sourceCount)
        throw std::invalid_argument("GLB external joint identities are incomplete");
    for(const auto &bone:identity.bones)
    {
        if(bone.source>=sourceCount||bone.bone>=foreignCount||sourceSeen[bone.source]||foreignSeen[bone.bone])
            throw std::invalid_argument("GLB external joint identity is invalid");
        sourceSeen[bone.source]=true;foreignSeen[bone.bone]=true;
    }
    if(std::find(foreignSeen.begin(),foreignSeen.end(),false)!=foreignSeen.end())
        throw std::invalid_argument("GLB external joint identities are incomplete");
}
void checkMaterials(const Object3DAsset &source,const Object3DAsset &snapshot,const GlbExternalIdentity &identity)
{
    const auto sourceCount=source.data().materials.size(),foreignCount=snapshot.data().materials.size();
    if(identity.materials.size()!=sourceCount||foreignCount!=sourceCount)
        throw std::invalid_argument("GLB external material identities are incomplete");
    std::vector<bool> sourceSeen(sourceCount),foreignSeen(foreignCount);
    for(const auto &material:identity.materials)
    {
        if(material.source>=sourceCount||material.material>=foreignCount||sourceSeen[material.source]||foreignSeen[material.material])
            throw std::invalid_argument("GLB external material identity is invalid or ambiguous");
        sourceSeen[material.source]=true;foreignSeen[material.material]=true;
    }
}
void mapMesh(const Object3DMesh &source,const Object3DMesh &foreign,
             const GlbExternalMeshIdentity &provenance,ExternalMeshIdentity &identity)
{
    if(!provenance.pointsRestored||!source.topology||!foreign.topology)
        throw std::invalid_argument("GLB external source point identities are unavailable");
    const auto &original=*source.topology,&imported=*foreign.topology;
    if(original.points.size()!=imported.points.size()||original.points.size()>=NoTopologyIndex)
        throw std::invalid_argument("GLB external point count changed");
    const auto &definition=original.layout->definition(),&foreignDefinition=imported.layout->definition();
    const auto triangles=original.layout->triangles();
    if(triangles.size()%3||triangles.size()/3>=NoTopologyIndex||foreignDefinition.polygons.size()!=triangles.size()/3)
        throw std::invalid_argument("GLB external triangle count changed");
    std::map<Triangle,SourceTriangle> sourceTriangles;
    for(std::size_t i=0;i<triangles.size();i+=3)
    {
        SourceTriangle triangle;triangle.id=static_cast<std::uint32_t>(i/3);
        for(std::size_t j=0;j<3;++j)
        {
            const auto layout=definition.renderToLayout.at(triangles[i+j]);
            triangle.layouts[j]=layout;triangle.points[j]=definition.layouts.at(layout).position;
        }
        if(!sourceTriangles.emplace(sorted(triangle.points),triangle).second)
            throw std::invalid_argument("GLB external triangle point identity is ambiguous");
    }
    identity.source=provenance.source;identity.mesh=provenance.mesh;
    identity.topology=ExternalMeshTopology::RenderTriangles;
    identity.points.resize(original.points.size());std::iota(identity.points.begin(),identity.points.end(),0u);
    identity.polygons.clear();identity.corners.clear();identity.polygons.reserve(foreignDefinition.polygons.size());
    identity.corners.reserve(triangles.size());std::vector<bool> seen(triangles.size()/3);
    for(const auto &face:foreignDefinition.polygons)
    {
        if(face.size()!=3)throw std::invalid_argument("GLB external face is not a triangle");
        Triangle points{};
        for(std::size_t j=0;j<3;++j)points[j]=foreignDefinition.layouts.at(face[j]).position;
        const auto found=sourceTriangles.find(sorted(points));
        if(found==sourceTriangles.end()||seen[found->second.id]||!sameWinding(found->second.points,points))
            throw std::invalid_argument("GLB external triangle connectivity or winding changed");
        const auto &triangle=found->second;seen[triangle.id]=true;identity.polygons.push_back(triangle.id);
        for(const auto point:points)
        {
            const auto position=std::find(triangle.points.begin(),triangle.points.end(),point);
            if(position==triangle.points.end())throw std::invalid_argument("GLB external corner identity is invalid");
            identity.corners.push_back(triangle.layouts[static_cast<std::size_t>(position-triangle.points.begin())]);
        }
    }
    if(std::find(seen.begin(),seen.end(),false)!=seen.end())
        throw std::invalid_argument("GLB external triangle identities are incomplete");
}
} // namespace

Object3DExternalIdentity makeObject3DGlbExternalIdentity(const Object3DAsset &source,const Object3DAsset &snapshot,
    const GlbExternalIdentity &provenance,std::string_view expectedDocument)
{
    if(expectedDocument.size()>128)throw std::invalid_argument("GLB external document token exceeds its limit");
    object3d_detail::validateName(expectedDocument);
    if(provenance.document!=expectedDocument)throw std::invalid_argument("GLB external document identity mismatch");
    checkBones(source,snapshot,provenance);checkMaterials(source,snapshot,provenance);
    const auto &original=source.data(),&imported=snapshot.data();
    if(provenance.meshes.size()!=original.meshes.size()||imported.meshes.size()!=original.meshes.size())
        throw std::invalid_argument("GLB external mesh identities are incomplete");
    auto result=makeObject3DExternalIdentity(imported,std::string(expectedDocument));
    result.bones=provenance.bones;result.materials=provenance.materials;
    std::vector<bool> sourceSeen(original.meshes.size()),foreignSeen(imported.meshes.size());
    for(const auto &mesh:provenance.meshes)
    {
        if(mesh.source>=original.meshes.size()||mesh.mesh>=imported.meshes.size()||
           sourceSeen[mesh.source]||foreignSeen[mesh.mesh])
            throw std::invalid_argument("GLB external mesh identity is invalid");
        sourceSeen[mesh.source]=true;foreignSeen[mesh.mesh]=true;
        mapMesh(original.meshes[mesh.source],imported.meshes[mesh.mesh],mesh,result.meshes.at(mesh.mesh));
    }
    return result;
}
Object3DExternalIdentity makeObject3DGlbExternalIdentity(const Object3DAsset &source,const GlbImportResult &snapshot,
    std::string_view expectedDocument)
{
    if(!snapshot.externalIdentity)throw std::invalid_argument("GLB external edit provenance is unavailable");
    return makeObject3DGlbExternalIdentity(source,snapshot.object.get(),*snapshot.externalIdentity,expectedDocument);
}
} // namespace iiActorObject
