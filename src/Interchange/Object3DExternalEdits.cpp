#include "Object3DExternalEdits.h"
#include "MeshEditGraph.h"
#include "MeshGeometryCore.h"
#include "Object3DMorphChannels.h"
#include "SkeletalTransform.h"
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <type_traits>

namespace iiActorObject
{
using namespace character_detail;
namespace
{
std::size_t pointCount(const Object3DMesh &m){return m.topology?m.topology->points.size():m.geometry.vertices.size();}
Vector3 point(const Object3DMesh &m,std::size_t i){return m.topology?m.topology->points.at(i).position:m.geometry.vertices.at(i).position;}
std::span<const SkinWeight> weights(const Object3DMesh &m,std::size_t i){return m.topology?std::span(m.topology->points.at(i).weights):std::span(m.geometry.vertices.at(i).weights);}
std::vector<SkinWeight> &weights(Object3DMesh &m,std::size_t i){return m.topology?m.topology->points.at(i).weights:m.geometry.vertices.at(i).weights;}
Vector3 &point(Object3DMesh &m,std::size_t i){return m.topology?m.topology->points.at(i).position:m.geometry.vertices.at(i).position;}
std::span<const TextureCoordinate> uv(const Object3DMesh &m){return m.topology?std::span(m.topology->textureCoordinates):std::span(m.textureCoordinates);}
std::vector<TextureCoordinate> &uv(Object3DMesh &m){return m.topology?m.topology->textureCoordinates:m.textureCoordinates;}
std::uint32_t sourceNormalIndex(const Object3DMesh &m,std::uint32_t layout)
{return m.topology?m.topology->layout->definition().layouts.at(layout).normal:layout;}
Vector3 normal(const Object3DMesh &m,std::uint32_t index)
{return m.topology?m.topology->normals.at(index):m.geometry.vertices.at(index).normal;}
Vector3 &normal(Object3DMesh &m,std::uint32_t index)
{return m.topology?m.topology->normals.at(index):m.geometry.vertices.at(index).normal;}
std::vector<Vector3> geometricNormals(const Object3DMesh &m)
{
    if(m.topology)
    {
        MeshTopology surface;surface.layout=m.topology->layout;surface.normals=m.topology->normals;
        surface.textureCoordinates=m.topology->textureCoordinates;surface.points.reserve(m.topology->points.size());
        for(const auto &p:m.topology->points)surface.points.push_back({p.position,{}});
        recomputeTopologyNormals(surface);return surface.normals;
    }
    SkeletalMeshLod surface;surface.triangles=m.geometry.triangles;surface.vertices.reserve(m.geometry.vertices.size());
    for(const auto &v:m.geometry.vertices)surface.vertices.push_back({v.position,v.normal,{}});
    recomputeNormals(surface);std::vector<Vector3> result;result.reserve(surface.vertices.size());
    for(const auto &v:surface.vertices)result.push_back(v.normal);return result;
}
std::size_t additionalUvCount(const Object3DMesh &m)
{return m.topology?m.topology->additionalUvSets.size():m.additionalUvSets.size();}
std::string_view primaryUvName(const Object3DMesh &m)
{
    const auto &name=m.topology?m.topology->primaryUvName:m.primaryUvName;
    return name.empty()?std::string_view("UVMap"):std::string_view(name);
}
std::string_view additionalUvName(const Object3DMesh &m,std::size_t set)
{return m.topology?std::string_view(m.topology->additionalUvSets.at(set).name):std::string_view(m.additionalUvSets.at(set).name);}
std::optional<TextureCoordinate> additionalUv(const Object3DMesh &m,std::size_t set,std::uint32_t layout)
{
    if(m.topology)
    {
        const auto &uv=m.topology->additionalUvSets.at(set);const auto index=uv.layoutIndices.at(layout);
        return index==NoTopologyIndex?std::nullopt:std::optional(uv.coordinates.at(index));
    }
    return m.additionalUvSets.at(set).coordinates.at(layout);
}
std::uint32_t additionalUvIndex(const Object3DMesh &m,std::size_t set,std::uint32_t layout)
{return m.topology?m.topology->additionalUvSets.at(set).layoutIndices.at(layout):layout;}
std::vector<TextureCoordinate> &additionalUvValues(Object3DMesh &m,std::size_t set)
{return m.topology?m.topology->additionalUvSets.at(set).coordinates:m.additionalUvSets.at(set).coordinates;}
std::vector<Matrix4> world(const Object3DAsset &a){Pose p(a.skeleton());auto w=p.worldMatrices();return {w.begin(),w.end()};}
std::vector<Matrix4> world(const Object3DData &a){Pose p(std::make_shared<Skeleton>(a.bones));auto w=p.worldMatrices();return {w.begin(),w.end()};}
using Row=std::map<BoneIndex,double>;
Row row(std::span<const SkinWeight> values)
{Row r;if(values.empty())return {{0,1}};for(auto v:values)if(v.weight>0)r[v.bone]+=v.weight;return r;}
bool same(const Row &a,const Row &b,double e)
{auto get=[](const Row &r,BoneIndex i){auto v=r.find(i);return v==r.end()?0:v->second;};for(auto [i,w]:a)if(std::abs(w-get(b,i))>e)return false;for(auto [i,w]:b)if(std::abs(w-get(a,i))>e)return false;return true;}
bool sameImage(const Object3DData &a,std::size_t ai,const Object3DData &b,std::size_t bi,bool normal=false)
{
    const auto x=normal?a.materials[ai].normalImage:a.materials[ai].baseColorImage;
    const auto y=normal?b.materials[bi].normalImage:b.materials[bi].baseColorImage;
    if(x.has_value()!=y.has_value())return false;if(!x)return true;
    const auto &left=a.images.at(*x),&right=b.images.at(*y);return left.mimeType()==right.mimeType()&&std::equal(left.encoded().begin(),left.encoded().end(),right.encoded().begin(),right.encoded().end());
}
bool close(Vector3 a,Vector3 b,double e){return length(sub(a,b))<=e;}
bool cyclicEqual(std::span<const std::uint32_t> a,std::span<const std::uint32_t> b)
{if(a.size()!=b.size())return false;for(std::size_t start=0;start<b.size();++start){bool equal=true;for(std::size_t i=0;i<a.size();++i)if(a[i]!=b[(start+i)%b.size()]){equal=false;break;}if(equal)return true;}return a.empty();}
std::vector<TopologyMorph> morphs(const Object3DMesh &m)
{
    if(m.topology)return m.topology->morphTargets;std::vector<TopologyMorph> result;
    for(const auto &v:m.geometry.morphTargets){TopologyMorph t;t.name=v.name;t.normalDeltas.emplace();for(const auto &d:v.deltas){if(d.vertex>=NoTopologyIndex)throw std::length_error("External render morph IDs");t.deltas.push_back({static_cast<std::uint32_t>(d.vertex),d.position});if(d.normal!=Vector3{})t.normalDeltas->push_back({static_cast<std::uint32_t>(d.vertex),d.normal});}result.push_back(std::move(t));}return result;
}
void morphs(Object3DMesh &m,std::vector<TopologyMorph> values)
{
    if(m.topology){m.topology->morphTargets=std::move(values);return;}m.geometry.morphTargets.clear();
    for(auto &v:values){MorphTarget t;t.name=std::move(v.name);std::map<std::uint32_t,MorphDelta> deltas;for(auto d:v.deltas)deltas[d.point]={d.point,d.position,{}};if(v.normalDeltas)for(auto d:*v.normalDeltas){deltas[d.layout].vertex=d.layout;deltas[d.layout].normal=d.normal;}for(auto [id,d]:deltas){(void)id;t.deltas.push_back(d);}m.geometry.morphTargets.push_back(std::move(t));}
}
struct Face final {std::vector<std::uint32_t> layouts,points;std::vector<std::optional<TextureCoordinate>> uv;std::vector<std::optional<Vector3>> normals;std::size_t material=0;};
std::vector<Face> faces(const Object3DMesh &m)
{
    std::vector<Face> result;
    if(m.topology){const auto &d=m.topology->layout->definition();for(std::size_t f=0;f<d.polygons.size();++f){Face face;face.layouts=d.polygons[f];face.material=d.polygonMaterials.empty()?m.material:d.polygonMaterials[f];for(auto i:face.layouts){const auto l=d.layouts.at(i);face.points.push_back(l.position);face.uv.push_back(l.textureCoordinate==NoTopologyIndex?std::nullopt:std::optional(m.topology->textureCoordinates.at(l.textureCoordinate)));face.normals.push_back(l.normal==NoTopologyIndex?std::nullopt:std::optional(m.topology->normals.at(l.normal)));}result.push_back(std::move(face));}}
    else for(std::size_t f=0;f<m.geometry.triangles.size()/3;++f){Face face;face.material=m.triangleMaterials.empty()?m.material:m.triangleMaterials[f];for(unsigned c=0;c<3;++c){const auto i=m.geometry.triangles[f*3+c];face.layouts.push_back(i);face.points.push_back(i);face.uv.push_back(m.textureCoordinates.empty()?std::nullopt:std::optional(m.textureCoordinates.at(i)));face.normals.push_back(m.geometry.vertices[i].normal);}result.push_back(std::move(face));}return result;
}
std::vector<Face> renderTriangles(const Object3DMesh &mesh)
{
    if(!mesh.topology)return faces(mesh);
    const auto &topology=*mesh.topology;const auto &definition=topology.layout->definition();
    const auto triangles=topology.layout->triangles();
    if(triangles.size()%3||triangles.size()/3>=NoTopologyIndex)
        throw std::length_error("External source triangle count");
    std::vector<Face> result;result.reserve(triangles.size()/3);
    for(std::size_t i=0;i<triangles.size();i+=3)
    {
        Face face;face.layouts.reserve(3);face.points.reserve(3);
        for(std::size_t corner=0;corner<3;++corner)
        {
            const auto layout=definition.renderToLayout.at(triangles[i+corner]);
            face.layouts.push_back(layout);face.points.push_back(definition.layouts.at(layout).position);
        }
        result.push_back(std::move(face));
    }
    return result;
}
struct MeshIndex final
{
    const ExternalMeshIdentity *identity=nullptr;
    std::vector<std::size_t> points,polygons,offsets;
    std::map<std::string,std::size_t> morphs;
};
struct Index final
{
    explicit Index(const Object3DAsset &asset):object(asset){}
    const Object3DAsset &object;
    std::vector<BoneIndex> bones,toSource;
    std::vector<std::size_t> materials,toMaterial;
    std::vector<MeshIndex> meshes;
    std::vector<Matrix4> worlds;
};
Index index(const Object3DAsset &source,Object3DExternalSnapshot snapshot,std::string_view document)
{
    const auto &s=source.data(),&a=snapshot.object.data();const auto &id=snapshot.identity;
    if(id.document!=document)throw std::invalid_argument("External document identity mismatch");
    Index out(snapshot.object);out.worlds=world(snapshot.object);out.bones.assign(s.bones.size(),NoBone);out.toSource.assign(a.bones.size(),NoBone);
    if(!source.hasRig()&&id.bones.empty()&&a.bones.size()==1){out.bones[0]=0;out.toSource[0]=0;}
    else for(auto b:id.bones){if(b.source>=s.bones.size()||b.bone>=a.bones.size()||out.bones[b.source]!=NoBone||out.toSource[b.bone]!=NoBone)throw std::invalid_argument("Invalid external joint identity");out.bones[b.source]=b.bone;out.toSource[b.bone]=b.source;}
    if(std::find(out.bones.begin(),out.bones.end(),NoBone)!=out.bones.end())throw std::invalid_argument("External snapshots require all source joints");
    out.materials.assign(s.materials.size(),NoBone);out.toMaterial.assign(a.materials.size(),NoBone);
    for(auto m:id.materials){if(m.source>=s.materials.size()||m.material>=a.materials.size()||out.materials[m.source]!=NoBone||out.toMaterial[m.material]!=NoBone)throw std::invalid_argument("Invalid external material identity");out.materials[m.source]=m.material;out.toMaterial[m.material]=m.source;}
    if(std::find(out.materials.begin(),out.materials.end(),NoBone)!=out.materials.end()||std::find(out.toMaterial.begin(),out.toMaterial.end(),NoBone)!=out.toMaterial.end())throw std::invalid_argument("External snapshots require matching source materials");
    out.meshes.resize(s.meshes.size());std::set<std::size_t> foreignMeshes;
    for(const auto &m:id.meshes)
    {
        if(m.source>=s.meshes.size()||m.mesh>=a.meshes.size()||out.meshes[m.source].identity||!foreignMeshes.insert(m.mesh).second)throw std::invalid_argument("Invalid external mesh identity");
        if(m.topology!=ExternalMeshTopology::SourcePolygons&&m.topology!=ExternalMeshTopology::RenderTriangles)
            throw std::invalid_argument("Invalid external topology mapping");
        auto &v=out.meshes[m.source];v.identity=&m;const auto &src=s.meshes[m.source],&mesh=a.meshes[m.mesh];
        if(m.points.size()!=pointCount(mesh)||m.points.size()!=pointCount(src))throw std::invalid_argument("External source point count changed");v.points.assign(pointCount(src),NoBone);
        for(std::size_t p=0;p<m.points.size();++p){const auto target=m.points[p];if(target>=v.points.size()||v.points[target]!=NoBone)throw std::invalid_argument("Duplicate or invalid external source point");v.points[target]=p;}
        const auto sourceFaces=m.topology==ExternalMeshTopology::RenderTriangles?renderTriangles(src):faces(src);
        const auto foreignFaces=faces(mesh);v.polygons.assign(sourceFaces.size(),NoBone);
        if(m.polygons.size()!=foreignFaces.size())throw std::invalid_argument("External face identity dimensions");std::size_t offset=0;
        for(std::size_t f=0;f<foreignFaces.size();++f)
        {
            const auto target=m.polygons[f];const auto &face=foreignFaces[f];if(target>=sourceFaces.size()||v.polygons[target]!=NoBone||face.points.size()!=sourceFaces[target].points.size()||face.points.size()>m.corners.size()-std::min(offset,m.corners.size()))throw std::invalid_argument("External source topology changed");
            v.polygons[target]=f;v.offsets.push_back(offset);std::vector<std::uint32_t> supplied;
            for(std::size_t c=0;c<face.points.size();++c){const auto layout=m.corners[offset+c];const auto &original=sourceFaces[target];auto found=std::find(original.layouts.begin(),original.layouts.end(),layout);if(found==original.layouts.end()||m.points.at(face.points[c])!=original.points[found-original.layouts.begin()])throw std::invalid_argument("External source corner identity mismatch");supplied.push_back(layout);}
            auto sorted=supplied,expected=sourceFaces[target].layouts;std::sort(sorted.begin(),sorted.end());std::sort(expected.begin(),expected.end());if(sorted!=expected)throw std::invalid_argument("External corner connectivity changed");offset+=face.points.size();
        }
        if(offset!=m.corners.size())throw std::invalid_argument("Trailing external corner identities");
        if(m.topology==ExternalMeshTopology::RenderTriangles&&
           std::find(v.polygons.begin(),v.polygons.end(),NoBone)!=v.polygons.end())
            throw std::invalid_argument("External snapshots require every source render triangle");
        const auto targets=morphs(mesh);const auto groups=object3d_detail::logicalMorphGroups(mesh);
        std::map<std::string,std::size_t> groupIndices;for(std::size_t i=0;i<groups.size();++i)groupIndices.emplace(groups[i].morph.name,i);
        std::set<std::size_t> morphIndices,assignedGroups;
        for(const auto &target:m.morphs)
        {
            if(target.morph>=targets.size()||target.source.empty()||target.source.size()>4096||target.source.find('\0')!=std::string::npos||!morphIndices.insert(target.morph).second)
                throw std::invalid_argument("Invalid external morph identity");
            const auto group=groupIndices.at(targets[target.morph].name);
            if(!assignedGroups.insert(group).second||!v.morphs.emplace(target.source,group).second)
                throw std::invalid_argument("Invalid external morph identity");
        }
        if(assignedGroups.size()!=groups.size())throw std::invalid_argument("Unmapped external morphs");
    }
    if(foreignMeshes.size()!=a.meshes.size()||id.meshes.size()!=s.meshes.size())throw std::invalid_argument("External snapshots require all source meshes");
    return out;
}
BoneIndex parent(const Index &index,BoneIndex source)
{auto p=index.object.data().bones[index.bones[source]].parent;while(p!=NoBone){if(index.toSource[p]!=NoBone)return index.toSource[p];p=index.object.data().bones[p].parent;}return NoBone;}
std::vector<SkinWeight> mappedWeights(const Index &i,std::span<const SkinWeight> values)
{Row result;for(auto w:values)if(w.weight>0){const auto source=i.toSource.at(w.bone);if(source==NoBone)throw std::invalid_argument("Unmapped external skin influence");result[source]+=w.weight;}std::vector<SkinWeight> out;for(auto [b,w]:result)out.push_back({b,w});if(!out.empty())normalizeSkinWeights(out,i.bones.size());return out;}
std::map<std::uint32_t,Vector3> mappedMorph(const TopologyMorph &m,const ExternalMeshIdentity &id)
{std::map<std::uint32_t,Vector3> out;for(auto d:m.deltas)out[id.points.at(d.point)]=d.position;return out;}
std::map<std::uint32_t,Vector3> normalMorph(const Object3DMesh &mesh,const TopologyMorph &m)
{std::map<std::uint32_t,Vector3> result;if(mesh.topology){for(auto v:topologyMorphNormalDeltas(*mesh.topology,m))result[v.layout]=v.normal;}else if(m.normalDeltas)for(auto v:*m.normalDeltas)result[v.layout]=v.normal;return result;}
std::vector<TopologyMorph> logicalMorphs(const Object3DMesh &mesh)
{
    std::vector<TopologyMorph> result;
    for(auto &group:object3d_detail::logicalMorphGroups(mesh))result.push_back(std::move(group.morph));
    return result;
}
std::map<std::uint32_t,Vector3> groupNormals(const Object3DMesh &mesh,std::vector<TopologyMorph> records)
{
    if(records.empty())return {};
    for(auto &record:records)record.name="external-morph-group";
    const auto groups=object3d_detail::logicalMorphGroups(mesh,records);
    return normalMorph(mesh,groups.front().morph);
}
bool sameMorphRecord(const TopologyMorph &a,const TopologyMorph &b,double positionTolerance,double attributeTolerance)
{
    const auto equal=[](const auto &left,const auto &right,auto key,auto value,double tolerance)
    {
        std::map<std::uint32_t,Vector3> difference;
        for(const auto &v:left){const auto item=value(v);if(!finite(item)||!difference.emplace(key(v),item).second)throw std::invalid_argument("Invalid current morph record");}
        std::set<std::uint32_t> seen;
        for(const auto &v:right)
        {
            const auto item=value(v);if(!finite(item)||!seen.insert(key(v)).second)throw std::invalid_argument("Invalid current morph record");
            difference[key(v)]=sub(difference[key(v)],item);
        }
        return std::all_of(difference.begin(),difference.end(),[&](const auto &item){return length(item.second)<=tolerance;});
    };
    if(!equal(a.deltas,b.deltas,[](const auto &v){return v.point;},[](const auto &v){return v.position;},positionTolerance))return false;
    if(a.normalDeltas.has_value()!=b.normalDeltas.has_value())return false;
    return !a.normalDeltas||equal(*a.normalDeltas,*b.normalDeltas,[](const auto &v){return v.layout;},[](const auto &v){return v.normal;},attributeTolerance);
}
}
Object3DExternalIdentity makeObject3DExternalIdentity(const Object3DData &data,std::string document)
{
    if(document.empty()||document.size()>128||document.find('\0')!=std::string::npos)throw std::invalid_argument("Invalid external document token");Object3DExternalIdentity id;id.document=std::move(document);
    for(BoneIndex i=0;i<data.bones.size();++i)id.bones.push_back({i,i});for(std::size_t i=0;i<data.materials.size();++i)id.materials.push_back({i,i});
    for(std::size_t i=0;i<data.meshes.size();++i){const auto &mesh=data.meshes[i];ExternalMeshIdentity m;m.mesh=m.source=i;m.points.resize(pointCount(mesh));std::iota(m.points.begin(),m.points.end(),0u);const auto f=faces(mesh);for(std::size_t p=0;p<f.size();++p){m.polygons.push_back(static_cast<std::uint32_t>(p));m.corners.insert(m.corners.end(),f[p].layouts.begin(),f[p].layouts.end());}for(const auto &group:object3d_detail::logicalMorphGroups(mesh))m.morphs.push_back({group.representative,group.morph.name});id.meshes.push_back(std::move(m));}return id;
}
Object3DExternalReport applyObject3DExternalEdits(Object3DData &current,const Object3DAsset &source,Object3DExternalSnapshot baseline,Object3DExternalSnapshot edited,const Object3DExternalOptions &o)
{
    if(o.document.empty()||o.document.size()>128||o.document.find('\0')!=std::string::npos)throw std::invalid_argument("External edits require an expected document token");
    for(double v:{o.positionTolerance,o.attributeTolerance,o.matrixTolerance,o.referenceTolerance})if(!std::isfinite(v)||v<0||v>1e-2)throw std::invalid_argument("External edit tolerance");
    const auto &s=source.data();if(current.bones.size()!=s.bones.size()||current.meshes.size()!=s.meshes.size()||current.materials.size()!=s.materials.size()||current.kind!=s.kind)throw std::invalid_argument("Canonical source schema changed during external editing");
    for(std::size_t m=0;m<s.meshes.size();++m)
    {
        const auto &a=s.meshes[m],&b=current.meshes[m];
        if(a.topology.has_value()!=b.topology.has_value()||pointCount(a)!=pointCount(b)||uv(a).size()!=uv(b).size()||
           primaryUvName(a)!=primaryUvName(b)||additionalUvCount(a)!=additionalUvCount(b)||(a.topology&&a.topology->layout->definition()!=b.topology->layout->definition())||
           (!a.topology&&a.geometry.triangles!=b.geometry.triangles))
            throw std::invalid_argument("Canonical source topology changed during external editing");
        for(std::size_t set=0;set<additionalUvCount(a);++set)
        {
            if(additionalUvName(a,set)!=additionalUvName(b,set))throw std::invalid_argument("Canonical UV set identity changed");
            if(a.topology)
            {
                const auto &x=a.topology->additionalUvSets[set],&y=b.topology->additionalUvSets[set];
                if(x.layoutIndices!=y.layoutIndices||x.coordinates.size()!=y.coordinates.size())
                    throw std::invalid_argument("Canonical UV set source IDs changed");
            }
            else if(a.additionalUvSets[set].coordinates.size()!=b.additionalUvSets[set].coordinates.size())
                throw std::invalid_argument("Canonical UV set render IDs changed");
        }
    }
    const auto a=index(source,baseline,o.document),b=index(source,edited,o.document);
    for(std::size_t m=0;m<s.meshes.size();++m)
        if(a.meshes[m].identity->topology!=b.meshes[m].identity->topology||
           a.meshes[m].polygons.size()!=b.meshes[m].polygons.size())
            throw std::invalid_argument("External topology mappings differ");
    auto next=current;Object3DExternalReport result;std::vector<std::uint8_t> moved(s.meshes.size(),0),refresh(moved);const auto sourceWorld=world(source),currentWorld=world(current);auto desired=currentWorld;
    bool rigChanged=false;
    for(BoneIndex i=0;i<s.bones.size();++i)
    {
        if(parent(a,i)!=s.bones[i].parent||!close(a.worlds[a.bones[i]].transformPoint({}),sourceWorld[i].transformPoint({}),o.referenceTolerance))throw std::invalid_argument("External reference joint does not match the source snapshot");
        const auto baseParent=parent(a,i),newParent=parent(b,i);if(baseParent!=newParent){if(!o.overwriteConflicts&&current.bones[i].parent!=s.bones[i].parent&&current.bones[i].parent!=newParent)throw std::invalid_argument("Concurrent external joint parent conflict");next.bones[i].parent=newParent;++result.parents;rigChanged=true;}
        const auto &oldName=a.object.data().bones[a.bones[i]].name,&newName=b.object.data().bones[b.bones[i]].name;
        if(oldName!=newName){if(!o.overwriteConflicts&&current.bones[i].name!=s.bones[i].name&&current.bones[i].name!=newName)throw std::invalid_argument("Concurrent external joint name conflict");next.bones[i].name=newName;++result.names;rigChanged=true;}
        const auto delta=b.worlds[b.bones[i]]*a.worlds[a.bones[i]].invertedAffine();bool linear=false;for(std::size_t c=0;c<3;++c)for(std::size_t r=0;r<3;++r)if(std::abs(delta.elements()[c*4+r]-(c==r?1.0:0.0))>o.matrixTolerance)linear=true;
        auto offset=sub(b.worlds[b.bones[i]].transformPoint({}),a.worlds[a.bones[i]].transformPoint({}));if(length(offset)<=o.positionTolerance)offset={};
        if(linear||offset!=Vector3{}){auto matrix=linear?delta*currentWorld[i]:currentWorld[i];const auto wanted=add(currentWorld[i].transformPoint({}),offset);matrix=Transform{sub(wanted,matrix.transformPoint({})),{},{1,1,1}}.matrix()*matrix;desired[i]=matrix;++result.jointTransforms;rigChanged=true;}
    }
    if(rigChanged)
    {
        const Skeleton hierarchy(next.bones);
        for(auto i:hierarchy.evaluationOrder())
        {
            const auto p=next.bones[i].parent;const auto local=(p==NoBone?Matrix4{}:desired[p].invertedAffine())*desired[i];
            try{next.bones[i].bindTransform=object3d_detail::skeletalMatrixTransform(local,std::max(1e-9,o.matrixTolerance));}
            catch(const std::invalid_argument &error){throw std::invalid_argument("External joint '"+next.bones[i].name+"': "+error.what());}
            const auto rebuilt=next.bones[i].bindTransform.matrix();
            for(std::size_t c=0;c<16;++c)result.maximumJointMatrixError=std::max(result.maximumJointMatrixError,std::abs(local.elements()[c]-rebuilt.elements()[c])/std::max(1.0,std::abs(local.elements()[c])));
        }
        (void)Skeleton(next.bones);
    }
    for(std::size_t m=0;m<s.meshes.size();++m)
    {
        const auto &ma=a.meshes[m],&mb=b.meshes[m];const auto &base=a.object.data().meshes[ma.identity->mesh],&edit=b.object.data().meshes[mb.identity->mesh];auto &target=next.meshes[m];
        if(base.name!=edit.name){if(!o.overwriteConflicts&&current.meshes[m].name!=s.meshes[m].name&&current.meshes[m].name!=edit.name)throw std::invalid_argument("Concurrent external mesh name conflict");target.name=edit.name;++result.names;}
        for(std::size_t p=0;p<pointCount(target);++p)
        {
            if(!close(point(base,ma.points[p]),point(s.meshes[m],p),o.referenceTolerance))throw std::invalid_argument("External reference point does not match the source snapshot");
            const auto before=mappedWeights(a,weights(base,ma.points[p])),after=mappedWeights(b,weights(edit,mb.points[p]));
            if(!same(row(before),row(weights(s.meshes[m],p)),o.attributeTolerance))throw std::invalid_argument("External reference skin does not match source");
            const auto delta=sub(point(edit,mb.points[p]),point(base,ma.points[p]));if(length(delta)>o.positionTolerance){auto &v=point(target,p);v=add(v,delta);if(!finite(v))throw std::overflow_error("External point edit overflow");++result.positions;moved[m]=refresh[m]=1;}
            if(!same(row(before),row(after),o.attributeTolerance)){if(!o.overwriteConflicts&&!same(row(weights(current.meshes[m],p)),row(weights(s.meshes[m],p)),o.attributeTolerance)&&!same(row(weights(current.meshes[m],p)),row(after),o.attributeTolerance))throw std::invalid_argument("Concurrent external skin conflict");weights(target,p)=after;++result.weightRows;refresh[m]=1;}
        }
        if(primaryUvName(base)!=primaryUvName(s.meshes[m])||primaryUvName(edit)!=primaryUvName(s.meshes[m])||
           additionalUvCount(base)!=additionalUvCount(s.meshes[m])||additionalUvCount(edit)!=additionalUvCount(s.meshes[m]))
            throw std::invalid_argument("External UV set count changed");
        for(std::size_t set=0;set<additionalUvCount(base);++set)
            if(additionalUvName(base,set)!=additionalUvName(s.meshes[m],set)||additionalUvName(edit,set)!=additionalUvName(s.meshes[m],set))
                throw std::invalid_argument("External UV set identity changed");
        const auto af=faces(base),bf=faces(edit);std::map<std::uint32_t,TextureCoordinate> uvDeltas;
        std::vector<Vector3> baseGeometric,editGeometric,currentGeometric,targetGeometric;
        if(moved[m])
        {
            baseGeometric=geometricNormals(base);editGeometric=geometricNormals(edit);
            currentGeometric=geometricNormals(current.meshes[m]);targetGeometric=geometricNormals(target);
        }
        std::vector<std::map<std::uint32_t,TextureCoordinate>> extraUvDeltas(additionalUvCount(base));
        std::vector<std::array<std::uint32_t,3>> normalLayouts;
        struct NormalChange {Vector3 reference,desired;bool edited=false,inconsistent=false;};
        std::map<std::uint32_t,NormalChange> normalChanges;
        for(std::size_t f=0;f<ma.polygons.size();++f)
        {
            if((ma.polygons[f]==NoBone)!=(mb.polygons[f]==NoBone))throw std::invalid_argument("External face set changed");if(ma.polygons[f]==NoBone)continue;const auto fa=ma.polygons[f],fb=mb.polygons[f];const auto &ac=af[fa],&bc=bf[fb];const auto ao=ma.offsets[fa],bo=mb.offsets[fb];std::span<const std::uint32_t> ca(ma.identity->corners.data()+ao,ac.layouts.size()),cb(mb.identity->corners.data()+bo,bc.layouts.size());
            if(!cyclicEqual(ca,cb))throw std::invalid_argument("External face connectivity or winding changed");if(a.toMaterial.at(ac.material)!=b.toMaterial.at(bc.material))throw std::invalid_argument("External face material reassignment requires a topology edit");
            for(std::size_t c=0;c<ca.size();++c)
            {
                const auto e=std::find(cb.begin(),cb.end(),ca[c])-cb.begin();normalLayouts.push_back({ca[c],ac.layouts[c],bc.layouts[e]});
                if(ac.normals[c].has_value()!=bc.normals[e].has_value())throw std::invalid_argument("External normal layout changed");
                if(ac.normals[c])
                {
                    const auto id=sourceNormalIndex(s.meshes[m],ca[c]);
                    auto from=*ac.normals[c],to=*bc.normals[e];
                    if(moved[m])
                    {
                        const auto baseId=sourceNormalIndex(base,ac.layouts[c]),editId=sourceNormalIndex(edit,bc.layouts[e]);
                        if(baseId==NoTopologyIndex||editId==NoTopologyIndex)throw std::invalid_argument("External normal geometry is absent");
                        from=unit(rotateDirection(directionRotation(baseGeometric.at(baseId),from),editGeometric.at(editId)));
                    }
                    if(id==NoTopologyIndex)
                    {
                        if(!close(from,to,o.attributeTolerance))throw std::invalid_argument("External normal creation requires source topology editing");
                    }
                    else
                    {
                        auto previous=normal(current.meshes[m],id);
                        if(moved[m])previous=unit(rotateDirection(directionRotation(currentGeometric.at(id),previous),targetGeometric.at(id)));
                        const bool edited=!close(from,to,o.attributeTolerance);
                        const auto desired=unit(rotateDirection(directionRotation(from,to),previous));
                        const auto [it,added]=normalChanges.emplace(id,NormalChange{*ac.normals[c],desired,edited,false});
                        if(!added&&(!close(it->second.reference,*ac.normals[c],o.referenceTolerance)||!close(it->second.desired,desired,o.attributeTolerance)))
                            it->second.inconsistent=true;
                        if(!added)it->second.edited|=edited;
                    }
                }
                for(std::size_t set=0;set<extraUvDeltas.size();++set)
                {
                    const auto before=additionalUv(base,set,ac.layouts[c]),after=additionalUv(edit,set,bc.layouts[e]);
                    const auto reference=additionalUv(s.meshes[m],set,ca[c]);
                    if(before.has_value()!=after.has_value()||before.has_value()!=reference.has_value())
                        throw std::invalid_argument("External UV set layout changed");
                    if(!before)continue;
                    if(std::abs(before->u-reference->u)>o.referenceTolerance||std::abs(before->v-reference->v)>o.referenceTolerance)
                        throw std::invalid_argument("External reference UV set differs from source");
                    const TextureCoordinate delta{after->u-before->u,after->v-before->v};
                    const auto id=additionalUvIndex(s.meshes[m],set,ca[c]);
                    const auto [it,added]=extraUvDeltas[set].emplace(id,delta);
                    if(!added&&(std::abs(it->second.u-delta.u)>o.attributeTolerance||std::abs(it->second.v-delta.v)>o.attributeTolerance))
                        throw std::invalid_argument("External UV set split requires a new source layout");
                }
                const auto id=s.meshes[m].topology?s.meshes[m].topology->layout->definition().layouts.at(ca[c]).textureCoordinate:(uv(s.meshes[m]).empty()?NoTopologyIndex:ca[c]);
                if(ac.uv[c].has_value()!=bc.uv[e].has_value())throw std::invalid_argument("External UV layout changed");
                if(!ac.uv[c])
                {
                    if(id!=NoTopologyIndex)throw std::invalid_argument("External reference UV is absent from source layout");
                    continue;
                }
                const TextureCoordinate delta{bc.uv[e]->u-ac.uv[c]->u,bc.uv[e]->v-ac.uv[c]->v};
                if(id==NoTopologyIndex){if(std::abs(delta.u)>o.attributeTolerance||std::abs(delta.v)>o.attributeTolerance)throw std::invalid_argument("External UV creation requires source topology editing");continue;}
                const auto sourceUvs=uv(s.meshes[m]);
                if(id>=sourceUvs.size())throw std::out_of_range("External source UV index");
                const auto reference=sourceUvs[id];
                if(std::abs(ac.uv[c]->u-reference.u)>o.referenceTolerance||std::abs(ac.uv[c]->v-reference.v)>o.referenceTolerance)
                    throw std::invalid_argument("External reference UV differs from source");
                const auto [it,added]=uvDeltas.emplace(id,delta);if(!added&&(std::abs(it->second.u-delta.u)>o.attributeTolerance||std::abs(it->second.v-delta.v)>o.attributeTolerance))throw std::invalid_argument("External UV split requires a new source layout");
            }
        }
        for(auto [id,delta]:uvDeltas)if(std::abs(delta.u)>o.attributeTolerance||std::abs(delta.v)>o.attributeTolerance){auto &v=uv(target).at(id);v.u+=delta.u;v.v+=delta.v;if(!std::isfinite(v.u)||!std::isfinite(v.v))throw std::overflow_error("External UV overflow");++result.uvs;refresh[m]=1;}
        for(std::size_t set=0;set<extraUvDeltas.size();++set)
            for(const auto [id,delta]:extraUvDeltas[set])
                if(std::abs(delta.u)>o.attributeTolerance||std::abs(delta.v)>o.attributeTolerance)
                {
                    auto &v=additionalUvValues(target,set).at(id);v.u+=delta.u;v.v+=delta.v;
                    if(!std::isfinite(v.u)||!std::isfinite(v.v))throw std::overflow_error("External UV set overflow");
                    ++result.uvs;refresh[m]=1;
                }
        const auto am=logicalMorphs(base),bm=logicalMorphs(edit),originals=morphs(s.meshes[m]);
        const auto sourceGroups=logicalMorphs(s.meshes[m]),currentTargets=morphs(current.meshes[m]);
        auto targets=morphs(target);std::set<std::string> names;for(const auto &[name,id]:ma.morphs){(void)id;names.insert(name);}for(const auto &[name,id]:mb.morphs){(void)id;names.insert(name);}
        for(const auto &name:names)
        {
            const auto sourceMorph=std::find_if(originals.begin(),originals.end(),[&](const auto &v){return v.name==name;});
            const auto sourceGroup=std::find_if(sourceGroups.begin(),sourceGroups.end(),[&](const auto &v){return v.name==name;});
            const TopologyMorph *aMorph=nullptr,*bMorph=nullptr;
            std::map<std::uint32_t,Vector3> before,after;
            if(auto i=ma.morphs.find(name);i!=ma.morphs.end())
            {
                aMorph=&am[i->second];before=mappedMorph(*aMorph,*ma.identity);
                if(sourceGroup==sourceGroups.end())throw std::invalid_argument("External reference contains an unknown source morph");
                std::map<std::uint32_t,Vector3> difference;for(auto v:sourceGroup->deltas)difference[v.point]=v.position;
                for(auto [id,v]:before)difference[id]=sub(difference[id],v);
                for(auto [id,v]:difference){(void)id;if(length(v)>o.referenceTolerance)throw std::invalid_argument("External reference morph differs from source");}
            }
            if(auto i=mb.morphs.find(name);i!=mb.morphs.end()){bMorph=&bm[i->second];after=mappedMorph(*bMorph,*mb.identity);}
            if(aMorph&&sourceGroup!=sourceGroups.end()&&(aMorph->normalDeltas||sourceGroup->normalDeltas))
            {
                const auto reference=normalMorph(s.meshes[m],*sourceGroup),observed=normalMorph(base,*aMorph);
                for(const auto ids:normalLayouts)
                {
                    const auto left=reference.find(ids[0]),right=observed.find(ids[1]);
                    const auto expected=left==reference.end()?Vector3{}:left->second;
                    const auto value=right==observed.end()?Vector3{}:right->second;
                    if(!close(expected,value,o.referenceTolerance))throw std::invalid_argument("External reference morph normals differ from source");
                }
            }
            std::map<std::uint32_t,Vector3> delta=after;for(auto [id,v]:before)delta[id]=sub(delta[id],v);bool changed=false;for(auto [id,v]:delta){(void)id;if(length(v)>o.positionTolerance)changed=true;}
            std::map<std::uint32_t,Vector3> normalDelta;
            if((aMorph&&aMorph->normalDeltas)||(bMorph&&bMorph->normalDeltas))
            {
                auto an=aMorph?normalMorph(base,*aMorph):std::map<std::uint32_t,Vector3>{},bn=bMorph?normalMorph(edit,*bMorph):std::map<std::uint32_t,Vector3>{};
                for(auto ids:normalLayouts){const auto value=sub(bn[ids[2]],an[ids[1]]);const auto [it,inserted]=normalDelta.emplace(ids[0],value);if(!inserted&&!close(it->second,value,o.attributeTolerance))throw std::invalid_argument("External morph normal split requires a new source layout");if(length(value)>o.attributeTolerance)changed=true;}
            }
            const bool renamed=aMorph&&bMorph&&aMorph->name!=bMorph->name,existenceChanged=static_cast<bool>(aMorph)!=static_cast<bool>(bMorph);if(!changed&&!renamed&&!existenceChanged)continue;
            std::set<std::uint32_t> sourceIds;std::size_t sourceUnnamed=0;
            for(const auto &v:originals)if(v.name==name)
            {if(v.sourceTarget==NoTopologyIndex)++sourceUnnamed;else sourceIds.insert(v.sourceTarget);}
            auto belongs=[&](const TopologyMorph &v)
            {
                if(v.name==name)return true;
                if(v.sourceTarget!=NoTopologyIndex&&sourceIds.contains(v.sourceTarget))return true;
                return v.sourceTarget==NoTopologyIndex&&(sourceUnnamed||sourceMorph==originals.end())&&
                       renamed&&v.name==bMorph->name;
            };
            for(const auto &original:originals)if(original.name==name&&original.sourceTarget!=NoTopologyIndex&&
                std::none_of(targets.begin(),targets.end(),[&](const auto &v){return v.sourceTarget==original.sourceTarget;}))
            {
                if(!o.overwriteConflicts)throw std::invalid_argument("Concurrent source morph removal conflict");
                targets.push_back(original);
            }
            std::vector<std::size_t> members;for(std::size_t i=0;i<targets.size();++i)if(belongs(targets[i]))members.push_back(i);
            const auto unnamed=std::count_if(members.begin(),members.end(),[&](auto i){return targets[i].sourceTarget==NoTopologyIndex;});
            if(static_cast<std::size_t>(unnamed)<sourceUnnamed)
            {
                if(!o.overwriteConflicts)throw std::invalid_argument("Concurrent source morph removal conflict");
                std::size_t skip=static_cast<std::size_t>(unnamed);
                for(const auto &original:originals)if(original.name==name&&original.sourceTarget==NoTopologyIndex)
                {if(skip){--skip;continue;}targets.push_back(original);members.push_back(targets.size()-1);}
            }
            if(members.empty())
            {
                if(sourceMorph!=originals.end()&&!o.overwriteConflicts)throw std::invalid_argument("Concurrent source morph removal conflict");
                TopologyMorph created;if(sourceMorph!=originals.end())created=*sourceMorph;else created.name=name;
                targets.push_back(std::move(created));members.push_back(targets.size()-1);
            }
            for(const auto index:members)
                if(!o.overwriteConflicts&&targets[index].name!=name&&(!renamed||targets[index].name!=bMorph->name))
                    throw std::invalid_argument("Concurrent external morph name conflict");
            if(renamed)
            {
                for(std::size_t i=0;i<targets.size();++i)
                    if(targets[i].name==bMorph->name&&std::find(members.begin(),members.end(),i)==members.end())
                        throw std::invalid_argument("External morph rename collides with another logical channel");
                for(const auto index:members)targets[index].name=bMorph->name;
                ++result.names;
            }
            auto representative=members.front();
            if(sourceMorph!=originals.end()&&sourceMorph->sourceTarget!=NoTopologyIndex)
                for(const auto index:members)if(targets[index].sourceTarget==sourceMorph->sourceTarget){representative=index;break;}
            if(!mb.morphs.contains(name))
            {
                if(!o.overwriteConflicts&&sourceMorph!=originals.end())
                {
                    std::vector<std::size_t> unnamedMembers;
                    for(const auto index:members)
                    {
                        const auto id=targets[index].sourceTarget;
                        if(id==NoTopologyIndex)unnamedMembers.push_back(index);
                        else if(!sourceIds.contains(id))throw std::invalid_argument("Concurrent logical morph additions conflict with deletion");
                    }
                    if(unnamedMembers.size()>sourceUnnamed)throw std::invalid_argument("Concurrent logical morph additions conflict with deletion");
                    std::size_t unnamedIndex=0;
                    for(const auto &original:originals)if(original.name==name)
                    {
                        const auto found=original.sourceTarget==NoTopologyIndex?
                            (unnamedIndex<unnamedMembers.size()?unnamedMembers[unnamedIndex++]:targets.size()):
                            static_cast<std::size_t>(std::find_if(targets.begin(),targets.end(),[&](const auto &v){return v.sourceTarget==original.sourceTarget;})-targets.begin());
                        if(found>=targets.size()||!sameMorphRecord(original,targets[found],o.positionTolerance,o.attributeTolerance))
                            throw std::invalid_argument("Concurrent logical morph edits conflict with deletion");
                    }
                }
                for(const auto index:members){targets[index].deltas.clear();if(targets[index].normalDeltas)targets[index].normalDeltas->clear();}
                ++result.morphs;refresh[m]=1;continue;
            }
            std::map<std::uint32_t,Vector3> values;for(auto v:targets[representative].deltas)values[v.point]=v.position;
            for(auto [id,v]:delta)if(length(v)>o.positionTolerance){values[id]=add(values[id],v);if(!finite(values[id]))throw std::overflow_error("External morph overflow");}
            targets[representative].deltas.clear();for(auto [id,v]:values)if(length(v)>1e-12)targets[representative].deltas.push_back({id,v});
            if(std::any_of(normalDelta.begin(),normalDelta.end(),[&](const auto &v){return length(v.second)>o.attributeTolerance;}))
            {
                std::vector<TopologyMorph> currentMembers;for(const auto &v:currentTargets)if(belongs(v))currentMembers.push_back(v);
                auto coefficients=groupNormals(current.meshes[m],std::move(currentMembers));
                for(auto [id,v]:normalDelta)if(length(v)>o.attributeTolerance)coefficients[id]=add(coefficients[id],v);
                std::vector<TopologyMorph> others;for(const auto index:members)if(index!=representative)others.push_back(targets[index]);
                for(const auto &[id,value]:groupNormals(target,std::move(others)))coefficients[id]=sub(coefficients[id],value);
                targets[representative].normalDeltas.emplace();
                for(const auto &[id,value]:coefficients)
                {if(!finite(value))throw std::overflow_error("External morph normal overflow");if(length(value)>1e-12)targets[representative].normalDeltas->push_back({id,value});}
            }
            ++result.morphs;refresh[m]=1;
        }
        if(refresh[m]){morphs(target,std::move(targets));object3d_detail::refreshEditedMesh(target,moved[m]!=0);}
        bool normalsChanged=false;
        for(const auto &[id,change]:normalChanges)
        {
            if(change.edited&&change.inconsistent)throw std::invalid_argument("External normal split requires a new source layout");
            if(change.edited&&!close(unit(change.reference),unit(normal(s.meshes[m],id)),o.referenceTolerance))
                throw std::invalid_argument("External reference normal differs from source");
            if(!close(change.desired,normal(target,id),o.attributeTolerance))
            {
                normal(target,id)=change.desired;++result.normals;normalsChanged=true;
            }
        }
        if(normalsChanged&&target.topology)object3d_detail::refreshEditedMesh(target,false);
    }
    for(std::size_t i=0;i<s.materials.size();++i)
    {
        if(!sameImage(s,i,a.object.data(),a.materials[i]))throw std::invalid_argument("External reference image differs from source; embedded image data is required");
        if(!sameImage(a.object.data(),a.materials[i],b.object.data(),b.materials[i]))throw std::invalid_argument("External image replacement needs an explicit resource edit");
        if(!sameImage(s,i,a.object.data(),a.materials[i],true))throw std::invalid_argument("External reference normal image differs from source");
        if(!sameImage(a.object.data(),a.materials[i],b.object.data(),b.materials[i],true))throw std::invalid_argument("External normal image replacement needs an explicit resource edit");
        const auto &before=a.object.data().materials[a.materials[i]],&after=b.object.data().materials[b.materials[i]];auto &target=next.materials[i];bool changed=false;
        const auto &original=s.materials[i],&local=current.materials[i];
        if(effectiveAlphaMode(before)!=effectiveAlphaMode(original)||before.surface.doubleSided!=original.surface.doubleSided||
           std::abs(before.surface.alphaCutoff-original.surface.alphaCutoff)>o.referenceTolerance)
            throw std::invalid_argument("External reference material surface differs from source");
        if(effectiveAlphaMode(before)!=effectiveAlphaMode(after))
        {
            if(!o.overwriteConflicts&&effectiveAlphaMode(local)!=effectiveAlphaMode(original)&&
               effectiveAlphaMode(local)!=effectiveAlphaMode(after))throw std::invalid_argument("Concurrent external alpha mode conflict");
            target.surface.alphaMode=after.surface.alphaMode;changed=true;
        }
        if(before.surface.doubleSided!=after.surface.doubleSided)
        {
            if(!o.overwriteConflicts&&local.surface.doubleSided!=original.surface.doubleSided&&
               local.surface.doubleSided!=after.surface.doubleSided)throw std::invalid_argument("Concurrent external sidedness conflict");
            target.surface.doubleSided=after.surface.doubleSided;changed=true;
        }
        if(std::abs(before.surface.alphaCutoff-after.surface.alphaCutoff)>o.attributeTolerance)
        {
            if(!o.overwriteConflicts&&std::abs(local.surface.alphaCutoff-original.surface.alphaCutoff)>o.attributeTolerance&&
               std::abs(local.surface.alphaCutoff-after.surface.alphaCutoff)>o.attributeTolerance)
                throw std::invalid_argument("Concurrent external alpha cutoff conflict");
            target.surface.alphaCutoff=after.surface.alphaCutoff;changed=true;
        }
        if(before.baseColorUvSet!=s.materials[i].baseColorUvSet)
            throw std::invalid_argument("External reference material UV set differs from source");
        if(before.baseColorUvSet!=after.baseColorUvSet)
        {
            if(!o.overwriteConflicts&&current.materials[i].baseColorUvSet!=s.materials[i].baseColorUvSet&&
               current.materials[i].baseColorUvSet!=after.baseColorUvSet)
                throw std::invalid_argument("Concurrent external material UV set conflict");
            target.baseColorUvSet=after.baseColorUvSet;changed=true;
        }
        if(before.normalUvSet!=s.materials[i].normalUvSet||std::abs(before.normalScale-s.materials[i].normalScale)>o.referenceTolerance)
            throw std::invalid_argument("External reference normal-map binding differs from source");
        if(!o.overwriteConflicts&&(before.normalUvSet!=after.normalUvSet||
           std::abs(after.normalScale-before.normalScale)>o.attributeTolerance)&&!sameImage(s,i,current,i,true))
            throw std::invalid_argument("Concurrent external normal image conflict");
        if(before.normalUvSet!=after.normalUvSet)
        {
            if(!o.overwriteConflicts&&current.materials[i].normalUvSet!=s.materials[i].normalUvSet&&
               current.materials[i].normalUvSet!=after.normalUvSet)
                throw std::invalid_argument("Concurrent external normal UV set conflict");
            target.normalUvSet=after.normalUvSet;changed=true;
        }
        if(std::abs(after.normalScale-before.normalScale)>o.attributeTolerance)
        {
            target.normalScale+=after.normalScale-before.normalScale;changed=true;
        }
        for(unsigned c=0;c<4;++c)if(std::abs(after.baseColor[c]-before.baseColor[c])>o.attributeTolerance){target.baseColor[c]+=after.baseColor[c]-before.baseColor[c];changed=true;}
        for(auto pair:{std::pair{&target.roughness,after.roughness-before.roughness},std::pair{&target.metallic,after.metallic-before.metallic}})if(std::abs(pair.second)>o.attributeTolerance){*pair.first+=pair.second;changed=true;}
        if(before.name!=after.name){if(!o.overwriteConflicts&&current.materials[i].name!=s.materials[i].name&&current.materials[i].name!=after.name)throw std::invalid_argument("Concurrent external material name conflict");target.name=after.name;++result.names;changed=true;}
        for(auto v:target.baseColor)if(!std::isfinite(v)||v<0||v>1)throw std::invalid_argument("External material color conflict");if(!std::isfinite(target.roughness)||!std::isfinite(target.metallic)||target.roughness<0||target.roughness>1||target.metallic<0||target.metallic>1||!std::isfinite(target.normalScale)||std::abs(target.normalScale)>std::numeric_limits<float>::max())throw std::invalid_argument("External material scalar conflict");if(changed)++result.materials;
    }
    for(const auto &mesh:next.meshes)
    {
        const auto checkUv=[&](std::size_t material)
        {
            const auto &m=next.materials.at(material);
            if(m.normalImage&&uv(mesh).empty())throw std::invalid_argument("Normal texture requires mesh UV coordinates");
            const auto check=[&](const std::string &name,const auto &image)
            {
                if(name.empty())return;
                if(!image)throw std::invalid_argument("Material UV set requires an image");
                if(!uv(mesh).empty()&&primaryUvName(mesh)==name)return;
                for(std::size_t set=0;set<additionalUvCount(mesh);++set)
                    if(additionalUvName(mesh,set)==name)return;
                throw std::invalid_argument("Material UV set is absent from an edited mesh");
            };
            check(m.baseColorUvSet,m.baseColorImage);check(m.normalUvSet,m.normalImage);
        };
        if(mesh.triangleMaterials.empty())checkUv(mesh.material);
        else for(const auto material:mesh.triangleMaterials)checkUv(material);
    }
    std::set<std::string> meshNames,materialNames;for(const auto &v:next.meshes)if(v.name.empty()||v.name.find('\0')!=std::string::npos||!meshNames.insert(v.name).second)throw std::invalid_argument("External mesh name collision");for(const auto &v:next.materials)if(v.name.empty()||v.name.find('\0')!=std::string::npos||!materialNames.insert(v.name).second)throw std::invalid_argument("External material name collision");
    if(result.hasEdits()){static_assert(std::is_nothrow_move_assignable_v<Object3DData>);current=std::move(next);result.applied=true;}return result;
}
} // namespace iiActorObject
