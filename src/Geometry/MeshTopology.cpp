#include "MeshTopologyCore.h"
#include "CharacterMath.h"
#include "MeshGeometryCore.h"
#include "Object3DNames.h"
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <unordered_set>

namespace iiActorObject
{
using namespace character_detail;
namespace
{
void polygon(std::vector<std::uint32_t> corners, const std::vector<SkinVertex> &vertices,
             std::vector<std::uint32_t> &triangles, bool flip)
{
    if (corners.size() < 3 || corners.size() > 256) throw std::invalid_argument("Invalid DNA polygon size");
    for (const auto corner : corners)
        if (corner >= vertices.size()) throw std::out_of_range("DNA polygon layout index");
    Vector3 normal{};
    for (std::size_t i=0; i<corners.size(); ++i)
    {
        const auto a = vertices[corners[i]].position, b = vertices[corners[(i+1)%corners.size()]].position;
        normal = add(normal,cross(a,b));
    }
    const auto dimensions = Vector3{std::abs(normal.x),std::abs(normal.y),std::abs(normal.z)};
    const auto drop = dimensions.x > dimensions.y ? (dimensions.x > dimensions.z ? 0 : 2) :
                                                  (dimensions.y > dimensions.z ? 1 : 2);
    const auto point = [&](std::uint32_t index)
    {
        const auto p = vertices[index].position;
        return drop == 0 ? std::array{p.y,p.z} : drop == 1 ? std::array{p.x,p.z} : std::array{p.x,p.y};
    };
    const auto area = [&](std::uint32_t ia,std::uint32_t ib,std::uint32_t ic)
    {
        const auto a=point(ia), b=point(ib), c=point(ic);
        return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);
    };
    double signedArea=0, extent=0;
    for (std::size_t i=0;i<corners.size();++i)
    {
        const auto a=point(corners[i]), b=point(corners[(i+1)%corners.size()]), origin=point(corners[0]);
        signedArea+=(a[0]-origin[0])*(b[1]-origin[1])-(b[0]-origin[0])*(a[1]-origin[1]);
        extent=std::max({extent,std::abs(a[0]-origin[0]),std::abs(a[1]-origin[1])});
    }
    const double epsilon=extent*extent*1e-12, sign=signedArea>0?1:-1;
    if (std::abs(signedArea)<=epsilon) throw std::invalid_argument("Degenerate DNA polygon");
    const auto append = [&](std::uint32_t a,std::uint32_t b,std::uint32_t c)
    { triangles.insert(triangles.end(),{a,flip?c:b,flip?b:c}); };
    while (corners.size()>3)
    {
        bool found=false;
        for (std::size_t i=0;i<corners.size();++i)
        {
            const auto a=corners[(i+corners.size()-1)%corners.size()], b=corners[i],
                       c=corners[(i+1)%corners.size()];
            if (sign*area(a,b,c)<=epsilon) continue;
            bool inside=false;
            for (const auto p:corners)
                if (p!=a&&p!=b&&p!=c && sign*area(a,b,p)>=-epsilon &&
                    sign*area(b,c,p)>=-epsilon && sign*area(c,a,p)>=-epsilon) { inside=true; break; }
            if (inside) continue;
            append(a,b,c);
            corners.erase(corners.begin()+static_cast<std::ptrdiff_t>(i));
            found=true; break;
        }
        if (!found) throw std::invalid_argument("DNA polygon is not a simple triangulable face");
    }
    append(corners[0],corners[1],corners[2]);
}
bool close(double a,double b)
{
    return std::abs(a-b)<=1e-12*std::max({1.,std::abs(a),std::abs(b)});
}
bool close(Vector3 a,Vector3 b) { return close(a.x,b.x)&&close(a.y,b.y)&&close(a.z,b.z); }
bool zero(Vector3 v) { return v.x==0&&v.y==0&&v.z==0; }
void validateMorphs(const MeshTopology &t)
{
    std::unordered_set<std::uint32_t> sourceIds;
    for (const auto &m:t.morphTargets)
    {
        if (m.name.empty()||m.name.size()>1024||m.name.find('\0')!=std::string::npos ||
            (m.sourceTarget!=NoTopologyIndex&&!sourceIds.insert(m.sourceTarget).second))
            throw std::invalid_argument("Invalid topology morph identity");
        std::unordered_set<std::uint32_t> seen;
        for (const auto &v:m.deltas)
            if (v.point>=t.points.size()||!seen.insert(v.point).second||!finite(v.position))
                throw std::invalid_argument("Invalid or duplicate topology morph point");
        seen.clear();
        if(m.normalDeltas)
            for(const auto &v:*m.normalDeltas)
                if(v.layout>=t.layout->definition().layouts.size()||!seen.insert(v.layout).second||!finite(v.normal))
                    throw std::invalid_argument("Invalid or duplicate topology morph normal layout");
    }
    std::size_t normalCount=0;
    for(const auto &m:t.morphTargets)if(m.normalDeltas)
    {
        if(m.normalDeltas->size()>t.maximumRenderMorphDeltas-normalCount)throw std::length_error("Source morph normal limit");
        normalCount+=m.normalDeltas->size();
    }
}
void validateSourceTangents(const TopologyTangentSource &source)
{
    constexpr std::size_t limit=8'000'000;
    const auto &d=source.definition;
    if(source.positions.size()!=d.positionCount||source.normals.size()!=d.normalCount||
       source.textureCoordinates.size()!=d.textureCoordinateCount||source.textureCoordinates.empty()||source.additionalUvSets.size()>31||
       source.layers.empty()||source.layers.size()>32||source.triangles.size()>limit||
       source.triangles.size()%3||source.trianglePolygons.size()!=source.triangles.size()/3)
        throw std::invalid_argument("Invalid source tangent snapshot dimensions");
    for(const auto p:source.positions)if(!finite(p))throw std::invalid_argument("Nonfinite source tangent position");
    for(const auto n:source.normals)if(!finite(n))throw std::invalid_argument("Nonfinite source tangent normal");
    for(const auto uv:source.textureCoordinates)if(!std::isfinite(uv.u)||!std::isfinite(uv.v))
        throw std::invalid_argument("Nonfinite source tangent UV");
    for(const auto &set:source.additionalUvSets)
    {
        if(set.layoutIndices.size()!=d.layouts.size())throw std::invalid_argument("Invalid source tangent UV layout");
        for(const auto uv:set.coordinates)if(!std::isfinite(uv.u)||!std::isfinite(uv.v))
            throw std::invalid_argument("Nonfinite source tangent UV set");
    }
    std::set<std::uint32_t> hidden;
    for(const auto f:d.hiddenPolygons)if(f>=d.polygons.size()||!hidden.insert(f).second)
        throw std::invalid_argument("Invalid source tangent hidden polygon");
    std::size_t corners=0;
    for(std::size_t f=0;f<d.polygons.size();++f)
    {
        if(hidden.contains(static_cast<std::uint32_t>(f)))continue;
        if(d.polygons[f].size()>limit-corners)throw std::length_error("Source tangent corner budget");
        corners+=d.polygons[f].size();
    }
    std::size_t stored=0;std::set<std::size_t> sets;
    for(const auto &layer:source.layers)
    {
        if(layer.uvSet>source.additionalUvSets.size()||!sets.insert(layer.uvSet).second||
           (layer.tangents.empty()&&layer.binormals.empty())||
           (!layer.tangents.empty()&&layer.tangents.size()!=corners)||
           (!layer.binormals.empty()&&layer.binormals.size()!=corners)||
           (!layer.tangentW.empty()&&(layer.tangents.empty()||layer.tangentW.size()!=corners))||
           (!layer.binormalW.empty()&&(layer.binormals.empty()||layer.binormalW.size()!=corners)))
            throw std::invalid_argument("Invalid source tangent layer dimensions");
        if(corners>limit-stored)throw std::length_error("Source tangent layer budget");stored+=corners;
        for(const auto v:layer.tangents)if(!finite(v))throw std::invalid_argument("Nonfinite source tangent");
        for(const auto v:layer.binormals)if(!finite(v))throw std::invalid_argument("Nonfinite source binormal");
        for(const auto v:layer.tangentW)if(!std::isfinite(v))throw std::invalid_argument("Nonfinite source tangent W");
        for(const auto v:layer.binormalW)if(!std::isfinite(v))throw std::invalid_argument("Nonfinite source binormal W");
    }
}
void validateGlbTangents(const TopologyGlbTangentSource &source)
{
    constexpr std::size_t maxLayouts=8'000'000,maxDeltas=16'000'000;
    const auto &d=source.definition;
    if(d.layouts.size()>maxLayouts||source.positions.size()!=d.positionCount||source.normals.size()!=d.normalCount||
       source.textureCoordinates.size()!=d.textureCoordinateCount||source.additionalUvSets.size()>31||
       source.triangles.size()>maxLayouts||source.triangles.size()%3||
       source.trianglePolygons.size()!=source.triangles.size()/3||source.tangents.empty()||
       source.tangents.size()>maxLayouts||source.morphTargets.size()!=source.morphTangents.size())
        throw std::invalid_argument("Invalid GLB tangent snapshot dimensions");
    for(const auto p:source.positions)if(!finite(p))throw std::invalid_argument("Nonfinite GLB tangent position");
    for(const auto n:source.normals)if(!finite(n))throw std::invalid_argument("Nonfinite GLB tangent normal");
    for(const auto uv:source.textureCoordinates)if(!std::isfinite(uv.u)||!std::isfinite(uv.v))
        throw std::invalid_argument("Nonfinite GLB tangent UV");
    for(const auto &uv:source.additionalUvSets)
    {
        if(uv.layoutIndices.size()!=d.layouts.size())throw std::invalid_argument("Invalid GLB tangent UV layout");
        for(const auto coordinate:uv.coordinates)if(!std::isfinite(coordinate.u)||!std::isfinite(coordinate.v))
            throw std::invalid_argument("Nonfinite GLB tangent UV set");
    }
    std::vector<bool> present(d.layouts.size());std::uint32_t previous=0;bool first=true;
    for(const auto &t:source.tangents)
    {
        const auto magnitude=length(t.direction);
        if(t.layout>=present.size()||(!first&&t.layout<=previous)||!finite(t.direction)||
           !std::isfinite(magnitude)||std::abs(magnitude-1)>1e-5||(t.handedness!=1&&t.handedness!=-1))
            throw std::invalid_argument("Invalid GLB source tangent");
        present[t.layout]=true;previous=t.layout;first=false;
    }
    std::size_t count=0,snapshotCount=0;
    for(std::size_t i=0;i<source.morphTangents.size();++i)
    {
        const auto &m=source.morphTargets[i];
        if(m.name.empty()||m.name.size()>1024||m.name.find('\0')!=std::string::npos)
            throw std::invalid_argument("Invalid GLB tangent morph name");
        if(m.deltas.size()>32'000'000-snapshotCount)throw std::length_error("GLB tangent morph snapshot budget");
        snapshotCount+=m.deltas.size();
        if(m.normalDeltas)
        {if(m.normalDeltas->size()>32'000'000-snapshotCount)throw std::length_error("GLB tangent morph snapshot budget");snapshotCount+=m.normalDeltas->size();}
        for(const auto &delta:m.deltas)
            if(delta.point>=source.positions.size()||!finite(delta.position))throw std::invalid_argument("Invalid GLB tangent morph position");
        if(m.normalDeltas)for(const auto &delta:*m.normalDeltas)
            if(delta.layout>=d.layouts.size()||!finite(delta.normal))throw std::invalid_argument("Invalid GLB tangent morph normal");
        const auto &deltas=source.morphTangents[i];
        if(deltas.size()>maxDeltas-count)throw std::length_error("GLB tangent morph budget");count+=deltas.size();
        first=true;previous=0;
        for(const auto &delta:deltas)
        {
            if(delta.layout>=present.size()||!present[delta.layout]||(!first&&delta.layout<=previous)||!finite(delta.direction))
                throw std::invalid_argument("Invalid GLB source morph tangent");
            previous=delta.layout;first=false;
        }
    }
    std::size_t material=0;first=true;
    for(const auto &binding:source.bindings)
    {
        if((!first&&binding.material<=material)||(!binding.normalMapped&&!binding.normalUvSet.empty()))
            throw std::invalid_argument("Invalid GLB tangent material binding");
        if(!binding.normalUvSet.empty())object3d_detail::validateName(binding.normalUvSet);
        material=binding.material;first=false;
    }
}
void dimensions(const MeshTopology &t)
{
    if (!t.layout) throw std::invalid_argument("Topology requires immutable source layout");
    const auto &d=t.layout->definition();
    if (t.points.size()!=d.positionCount || t.normals.size()!=d.normalCount ||
        t.textureCoordinates.size()!=d.textureCoordinateCount || t.maximumRenderMorphDeltas==0)
        throw std::invalid_argument("Topology attribute counts do not match source IDs");
    validateSkinningProfile(t.skinning,t.points.size());
    for (const auto &point:t.points)
        if (!finite(point.position)) throw std::invalid_argument("Nonfinite topology control point");
    for (const auto normal:t.normals)
        if (!finite(normal)) throw std::invalid_argument("Nonfinite topology normal");
    for (const auto uv:t.textureCoordinates)
        if (!std::isfinite(uv.u)||!std::isfinite(uv.v)) throw std::invalid_argument("Nonfinite topology UV");
    if(t.additionalUvSets.size()>31)throw std::length_error("Topology UV set limit");
    if(!t.primaryUvName.empty())object3d_detail::validateName(t.primaryUvName);
    if(t.textureCoordinates.empty()&&!t.additionalUvSets.empty())
        throw std::invalid_argument("Additional UV sets require a primary set");
    std::unordered_set<std::string> uvNames;
    if(!t.textureCoordinates.empty())uvNames.insert(t.primaryUvName.empty()?"UVMap":t.primaryUvName);
    for(const auto &set:t.additionalUvSets)
    {
        object3d_detail::validateName(set.name);
        if(!uvNames.insert(set.name).second||set.coordinates.size()>=NoTopologyIndex||set.layoutIndices.size()!=d.layouts.size())
            throw std::invalid_argument("Invalid topology UV set dimensions or name");
        for(const auto uv:set.coordinates)
            if(!std::isfinite(uv.u)||!std::isfinite(uv.v))throw std::invalid_argument("Nonfinite topology UV set value");
        for(const auto index:set.layoutIndices)
            if(index!=NoTopologyIndex&&index>=set.coordinates.size())throw std::out_of_range("Topology UV set index");
        for(const auto layout:d.renderToLayout)
            if(set.layoutIndices[layout]==NoTopologyIndex)throw std::invalid_argument("Visible topology layout lacks a UV set coordinate");
    }
    validateMorphs(t);
    if(t.sourceTangents)validateSourceTangents(*t.sourceTangents);
    if(t.sourceGlbTangents)validateGlbTangents(*t.sourceGlbTangents);
}
std::vector<Vector3> normals(const MeshTopology &t,std::span<const Vector3> points)
{
    const auto &d=t.layout->definition();
    std::vector<Vector3> result(d.normalCount);
    const auto triangles=t.layout->triangles();
    for (std::size_t i=0;i<triangles.size();i+=3)
    {
        const auto a=d.layouts[d.renderToLayout[triangles[i]]],
                   b=d.layouts[d.renderToLayout[triangles[i+1]]],
                   c=d.layouts[d.renderToLayout[triangles[i+2]]];
        const auto n=cross(sub(points[b.position],points[a.position]),sub(points[c.position],points[a.position]));
        if (!finite(n)) throw std::invalid_argument("Nonfinite topology face normal");
        for (const auto corner:{a,b,c})
            if (corner.normal!=NoTopologyIndex) result[corner.normal]=add(result[corner.normal],n);
    }
    for (std::size_t i=0;i<result.size();++i)
        result[i]=zero(result[i])?t.normals[i]:unit(result[i]);
    return result;
}
using PointDeltas=std::map<std::uint32_t,Vector3>;
std::map<std::string,PointDeltas> aggregate(const MeshTopology &t)
{
    std::map<std::string,PointDeltas> result;
    for (const auto &m:t.morphTargets)
    {
        auto &d=result[m.name];
        for (const auto &v:m.deltas) d[v.point]=add(d[v.point],v.position);
    }
    return result;
}
bool sameWeights(std::span<const SkinWeight> a,std::span<const SkinWeight> b)
{
    if (a.size()!=b.size()) return false;
    for (const auto &w:a)
    {
        const auto found=std::find_if(b.begin(),b.end(),[&](const auto &v) { return v.bone==w.bone; });
        if (found==b.end()||!close(w.weight,found->weight)) return false;
    }
    return true;
}
} // namespace
MeshTopologyLayout::MeshTopologyLayout(MeshTopologyDefinition definition,std::vector<std::uint32_t> triangles,
    std::vector<std::uint32_t> trianglePolygons)
    : definition_(std::move(definition)),triangles_(std::move(triangles)),trianglePolygons_(std::move(trianglePolygons)) {}
namespace
{
struct PreparedTopology final
{
    std::vector<bool> hidden;
    std::vector<std::uint32_t> inverse;
    std::vector<SkinVertex> rendered;
};
PreparedTopology prepareTopology(MeshTopologyDefinition &d,std::span<const TopologyControlPoint> points,bool renderVertices=true)
{
    if (points.empty()||points.size()!=d.positionCount||d.layouts.empty()||
        d.layouts.size()>NoTopologyIndex||d.polygons.empty())
        throw std::invalid_argument("Invalid topology source dimensions");
    for (const auto &p:points)
        if (!finite(p.position)) throw std::invalid_argument("Nonfinite source point");
    for (const auto v:d.layouts)
        if (v.position>=d.positionCount ||
            (d.normalCount?v.normal>=d.normalCount:v.normal!=NoTopologyIndex) ||
            (d.textureCoordinateCount?v.textureCoordinate>=d.textureCoordinateCount:v.textureCoordinate!=NoTopologyIndex))
            throw std::out_of_range("Topology layout attribute index");
    if (!d.polygonGroups.empty() && d.polygonGroups.size()!=d.polygons.size())
        throw std::invalid_argument("Topology face labels do not match source polygons");
    if (!d.polygonMaterials.empty() && d.polygonMaterials.size()!=d.polygons.size())
        throw std::invalid_argument("Topology face materials do not match source polygons");
    std::vector<bool> hidden(d.polygons.size(),false);
    for (const auto i:d.hiddenPolygons)
    {
        if (i>=hidden.size() || hidden[i]) throw std::invalid_argument("Invalid hidden source polygon");
        hidden[i]=true;
    }
    std::sort(d.hiddenPolygons.begin(),d.hiddenPolygons.end());
    std::vector<bool> used(d.layouts.size(),false);
    for (std::size_t f=0;f<d.polygons.size();++f)
    {
        const auto &face=d.polygons[f];
        if (face.size()<3||face.size()>256) throw std::invalid_argument("Invalid source polygon size");
        for (const auto i:face)
        {
            if (i>=d.layouts.size()) throw std::out_of_range("Topology polygon layout index");
            if (!hidden[f]) used[i]=true;
        }
    }
    if (d.renderToLayout.empty())
        for (std::size_t i=0;i<used.size();++i)
            if (used[i]) d.renderToLayout.push_back(static_cast<std::uint32_t>(i));
    std::vector<std::uint32_t> inverse(d.layouts.size(),NoTopologyIndex);
    std::vector<SkinVertex> rendered;
    for (std::size_t i=0;i<d.renderToLayout.size();++i)
    {
        const auto layout=d.renderToLayout[i];
        if (layout>=d.layouts.size()||!used[layout]||inverse[layout]!=NoTopologyIndex)
            throw std::invalid_argument("Invalid or duplicate render layout mapping");
        inverse[layout]=static_cast<std::uint32_t>(i);
        if(renderVertices)rendered.push_back({points[d.layouts[layout].position].position,{},{}});
    }
    return {std::move(hidden),std::move(inverse),std::move(rendered)};
}
}
std::shared_ptr<const MeshTopologyLayout> MeshTopologyLayout::create(
    MeshTopologyDefinition d,std::span<const TopologyControlPoint> points)
{
    auto [hidden,inverse,rendered]=prepareTopology(d,points);
    std::vector<std::uint32_t> triangles,trianglePolygons;
    for (std::size_t f=0;f<d.polygons.size();++f)
    {
        if (hidden[f]) continue;
        const auto &face=d.polygons[f];
        std::vector<std::uint32_t> corners;
        for (const auto layout:face)
        {
            if (inverse[layout]==NoTopologyIndex) throw std::invalid_argument("Source polygon has no render mapping");
            corners.push_back(inverse[layout]);
        }
        const auto start=triangles.size();polygon(std::move(corners),rendered,triangles,d.reverseWinding);
        trianglePolygons.insert(trianglePolygons.end(),(triangles.size()-start)/3,static_cast<std::uint32_t>(f));
    }
    if (triangles.empty()) throw std::invalid_argument("Topology has no visible surface");
    return std::shared_ptr<const MeshTopologyLayout>(new MeshTopologyLayout(std::move(d),std::move(triangles),std::move(trianglePolygons)));
}
std::shared_ptr<const MeshTopologyLayout> MeshTopologyLayout::restore(MeshTopologyDefinition d,
    std::span<const TopologyControlPoint> points,std::vector<std::uint32_t> triangles,std::vector<std::uint32_t> trianglePolygons)
{
    const auto prepared=prepareTopology(d,points,false);
    if(triangles.empty()||triangles.size()%3||trianglePolygons.size()!=triangles.size()/3)throw std::invalid_argument("Invalid stored topology triangles");
    std::vector<std::vector<std::size_t>> faces(d.polygons.size());
    for(std::size_t t=0;t<trianglePolygons.size();++t)
    {
        const auto face=trianglePolygons[t];if(face>=faces.size()||prepared.hidden[face])throw std::invalid_argument("Stored triangle references an invalid or hidden polygon");
        faces[face].push_back(t);
    }
    using Edge=std::pair<std::uint32_t,std::uint32_t>;
    for(std::size_t f=0;f<d.polygons.size();++f)
    {
        if(prepared.hidden[f])continue;const auto &polygon=d.polygons[f];const auto n=static_cast<std::uint32_t>(polygon.size());
        if(faces[f].size()!=n-2)throw std::invalid_argument("Stored polygon triangle count");
        std::map<std::uint32_t,std::uint32_t> ordinal;
        for(std::uint32_t i=0;i<n;++i)
        {
            const auto vertex=prepared.inverse[polygon[i]];
            if(vertex==NoTopologyIndex||!ordinal.emplace(vertex,i).second)throw std::invalid_argument("Stored polygon has missing or repeated corners");
        }
        std::map<Edge,std::array<unsigned,2>> edges;
        for(const auto triangle:faces[f])
        {
            std::array<std::uint32_t,3> p{};
            for(unsigned i=0;i<3;++i){const auto found=ordinal.find(triangles[triangle*3+i]);if(found==ordinal.end())throw std::invalid_argument("Stored triangle leaves its source polygon");p[i]=found->second;}
            if(p[0]==p[1]||p[1]==p[2]||p[0]==p[2]||(((p[1]+n-p[0])%n<(p[2]+n-p[0])%n)==d.reverseWinding))throw std::invalid_argument("Stored triangle corner orientation");
            for(unsigned i=0;i<3;++i){const auto a=p[i],b=p[(i+1)%3];++edges[{std::min(a,b),std::max(a,b)}][a<b?0:1];}
        }
        std::set<Edge> boundary;std::vector<Edge> diagonals;
        for(std::uint32_t i=0;i<n;++i)
        {
            auto a=i,b=(i+1)%n;if(d.reverseWinding)std::swap(a,b);const Edge edge{std::min(a,b),std::max(a,b)};boundary.insert(edge);
            const auto found=edges.find(edge);const auto direction=a<b?0u:1u;
            if(found==edges.end()||found->second[direction]!=1||found->second[1-direction]!=0)throw std::invalid_argument("Stored topology polygon boundary mismatch");
        }
        for(const auto &[edge,count]:edges)if(!boundary.contains(edge))
        {if(count[0]!=1||count[1]!=1)throw std::invalid_argument("Stored topology internal edge mismatch");diagonals.push_back(edge);}
        for(std::size_t i=0;i<diagonals.size();++i)for(std::size_t j=i+1;j<diagonals.size();++j)
        {
            const auto [a,b]=diagonals[i];const auto [c,e]=diagonals[j];
            if((a<c&&c<b&&b<e)||(c<a&&a<e&&e<b))throw std::invalid_argument("Stored topology has crossing diagonals");
        }
    }
    return std::shared_ptr<const MeshTopologyLayout>(new MeshTopologyLayout(std::move(d),std::move(triangles),std::move(trianglePolygons)));
}
const MeshTopologyDefinition &MeshTopologyLayout::definition() const noexcept { return definition_; }
std::span<const std::uint32_t> MeshTopologyLayout::triangles() const noexcept { return triangles_; }
std::span<const std::uint32_t> MeshTopologyLayout::trianglePolygons() const noexcept { return trianglePolygons_; }
void normalizeMeshTopology(MeshTopology &t,std::size_t boneCount)
{
    dimensions(t);
    for (auto &p:t.points)
        if (!p.weights.empty()) normalizeSkinWeights(p.weights,boneCount);
    const auto &d=t.layout->definition();
    for (const auto layout:d.renderToLayout)
    {
        const auto n=d.layouts[layout].normal;
        if (n!=NoTopologyIndex) t.normals[n]=unit(t.normals[n]);
    }

}
bool topologySourceTangentsValid(const MeshTopology &t)
{
    if(!t.layout||!t.sourceTangents)return false;
    const auto &s=*t.sourceTangents;
    // Callers may retain a mutable alias after attaching the shared snapshot.
    // Recheck it before any interchange path dereferences source layer data.
    validateSourceTangents(s);
    if(s.definition!=t.layout->definition()||s.positions.size()!=t.points.size()||
       s.normals.size()!=t.normals.size()||s.textureCoordinates!=t.textureCoordinates||
       s.primaryUvName!=t.primaryUvName||s.additionalUvSets!=t.additionalUvSets||
       !std::equal(s.triangles.begin(),s.triangles.end(),t.layout->triangles().begin(),t.layout->triangles().end())||
       !std::equal(s.trianglePolygons.begin(),s.trianglePolygons.end(),t.layout->trianglePolygons().begin(),t.layout->trianglePolygons().end()))return false;
    for(std::size_t i=0;i<s.positions.size();++i)if(s.positions[i]!=t.points[i].position)return false;
    // Repeated normalization of the same normal can differ by a few ULPs.
    for(std::size_t i=0;i<s.normals.size();++i)
        if(std::hypot(s.normals[i].x-t.normals[i].x,s.normals[i].y-t.normals[i].y,s.normals[i].z-t.normals[i].z)>1e-12)return false;
    return true;
}
bool topologyGlbTangentsValid(const MeshTopology &t)
{
    if(!t.layout||!t.sourceGlbTangents)return false;
    const auto &s=*t.sourceGlbTangents;
    // A caller may still hold a mutable alias to a shared snapshot. Recheck it
    // before the exporter uses layout IDs or emits its authored values.
    validateGlbTangents(s);
    if(s.definition!=t.layout->definition()||s.positions.size()!=t.points.size()||
       s.normals.size()!=t.normals.size()||s.textureCoordinates!=t.textureCoordinates||
       s.primaryUvName!=t.primaryUvName||s.additionalUvSets!=t.additionalUvSets||s.morphTargets!=t.morphTargets||
       !std::equal(s.triangles.begin(),s.triangles.end(),t.layout->triangles().begin(),t.layout->triangles().end())||
       !std::equal(s.trianglePolygons.begin(),s.trianglePolygons.end(),t.layout->trianglePolygons().begin(),t.layout->trianglePolygons().end()))return false;
    for(std::size_t i=0;i<s.positions.size();++i)if(s.positions[i]!=t.points[i].position)return false;
    for(std::size_t i=0;i<s.normals.size();++i)
        if(std::hypot(s.normals[i].x-t.normals[i].x,s.normals[i].y-t.normals[i].y,s.normals[i].z-t.normals[i].z)>1e-12)return false;
    return true;
}
void recomputeTopologyNormals(MeshTopology &t)
{
    dimensions(t);
    std::vector<Vector3> points;
    for (const auto &p:t.points) points.push_back(p.position);
    t.normals=normals(t,points);
}
namespace
{
SkeletalMeshLod baseSurface(const MeshTopology &t)
{
    const auto &d=t.layout->definition();SkeletalMeshLod result;result.skinning.method=t.skinning.method;
    for(const auto id:d.renderToLayout)
    {
        const auto v=d.layouts[id];const auto &point=t.points[v.position];
        result.vertices.push_back({point.position,v.normal==NoTopologyIndex?Vector3{0,0,1}:unit(t.normals[v.normal]),
            point.weights.empty()?std::vector<SkinWeight>{{0,1}}:point.weights});
        if(t.skinning.method==SkinningMethod::Blended)result.skinning.blendWeights.push_back(t.skinning.blendWeights[v.position]);
    }
    const auto triangles=t.layout->triangles();result.triangles.assign(triangles.begin(),triangles.end());
    if(!d.normalCount)recomputeNormals(result);return result;
}
using LayoutNormals=std::map<std::uint32_t,Vector3>;
LayoutNormals derivedNormals(const MeshTopology &t,const PointDeltas &deltas,const SkeletalMeshLod &base)
{
    LayoutNormals result;if(deltas.empty())return result;
    const auto &d=t.layout->definition();std::vector<Vector3> points;points.reserve(t.points.size());
    for(std::size_t p=0;p<t.points.size();++p)
    {
        const auto found=deltas.find(static_cast<std::uint32_t>(p));
        auto position=t.points[p].position;if(found!=deltas.end())position=add(position,found->second);
        if(!finite(position))throw std::invalid_argument("Morph position overflow");points.push_back(position);
    }
    const auto triangles=t.layout->triangles();
    if(d.normalCount)
    {
        const auto target=normals(t,points);std::vector<bool> affected(d.normalCount,false);
        for(std::size_t i=0;i<triangles.size();i+=3)
        {
            bool changed=false;
            for(unsigned j=0;j<3;++j)
            {
                const auto point=d.layouts[d.renderToLayout[triangles[i+j]]].position;
                const auto found=deltas.find(point);if(found!=deltas.end()&&!zero(found->second))changed=true;
            }
            if(changed)for(unsigned j=0;j<3;++j)affected[d.layouts[d.renderToLayout[triangles[i+j]]].normal]=true;
        }
        for(std::size_t r=0;r<d.renderToLayout.size();++r)
        {
            const auto layout=d.renderToLayout[r],normal=d.layouts[layout].normal;
            if(affected[normal])
            {
                const auto delta=sub(target[normal],base.vertices[r].normal);if(!zero(delta))result.emplace(layout,delta);
            }
        }
    }
    else
    {
        std::vector<Vector3> sums(base.vertices.size());
        for(std::size_t i=0;i<triangles.size();i+=3)
        {
            const auto a=triangles[i],b=triangles[i+1],c=triangles[i+2];
            const auto pa=points[d.layouts[d.renderToLayout[a]].position],pb=points[d.layouts[d.renderToLayout[b]].position],pc=points[d.layouts[d.renderToLayout[c]].position];
            const auto n=cross(sub(pb,pa),sub(pc,pa));if(!finite(n))throw std::invalid_argument("Nonfinite morph surface normal");
            for(auto vertex:{a,b,c})sums[vertex]=add(sums[vertex],n);
        }
        for(std::size_t r=0;r<sums.size();++r)
        {
            const auto target=zero(sums[r])?base.vertices[r].normal:unit(sums[r]);
            const auto delta=sub(target,base.vertices[r].normal);if(!zero(delta))result.emplace(d.renderToLayout[r],delta);
        }
    }
    return result;
}
}
std::vector<TopologyNormalDelta> topologyMorphNormalDeltas(const MeshTopology &t,const TopologyMorph &morph)
{
    dimensions(t);
    if(morph.normalDeltas)
    {
        if(morph.normalDeltas->size()>t.maximumRenderMorphDeltas)throw std::length_error("Source morph normal limit");
        std::unordered_set<std::uint32_t> seen;
        for(const auto &v:*morph.normalDeltas)
            if(v.layout>=t.layout->definition().layouts.size()||!seen.insert(v.layout).second||!finite(v.normal))
                throw std::invalid_argument("Invalid morph normal layout");
        return *morph.normalDeltas;
    }
    PointDeltas positions;
    for(const auto &v:morph.deltas)
        if(v.point>=t.points.size()||!finite(v.position)||!positions.emplace(v.point,v.position).second)
            throw std::invalid_argument("Invalid morph point");
    std::vector<TopologyNormalDelta> result;
    for(const auto &[layout,normal]:derivedNormals(t,positions,baseSurface(t)))result.push_back({layout,normal});return result;
}
TopologyRenderData renderMeshTopology(const MeshTopology &t)
{
    dimensions(t);const auto &d=t.layout->definition();TopologyRenderData result;result.geometry=baseSurface(t);
    result.primaryUvName=t.primaryUvName;
    for(const auto id:d.renderToLayout)
    {
        const auto uv=d.layouts[id].textureCoordinate;if(uv!=NoTopologyIndex)result.textureCoordinates.push_back(t.textureCoordinates[uv]);
    }
    for(const auto &source:t.additionalUvSets)
    {
        TextureCoordinateSet set;set.name=source.name;set.coordinates.reserve(d.renderToLayout.size());
        for(const auto layout:d.renderToLayout)set.coordinates.push_back(source.coordinates[source.layoutIndices[layout]]);
        result.additionalUvSets.push_back(std::move(set));
    }
    if(!d.polygonMaterials.empty())for(const auto face:t.layout->trianglePolygons())result.triangleMaterials.push_back(d.polygonMaterials[face]);
    const auto channels=aggregate(t);std::size_t total=0;std::unordered_set<std::string_view> seen;
    for(const auto &source:t.morphTargets)
    {
        if(!seen.insert(source.name).second)continue;
        PointDeltas derivedPositions;LayoutNormals explicitNormals;
        for(const auto &record:t.morphTargets)if(record.name==source.name)
        {
            if(record.normalDeltas)for(const auto &v:*record.normalDeltas)explicitNormals[v.layout]=add(explicitNormals[v.layout],v.normal);
            else for(const auto &v:record.deltas)derivedPositions[v.point]=add(derivedPositions[v.point],v.position);
        }
        auto normalValues=derivedNormals(t,derivedPositions,result.geometry);
        for(const auto &[layout,value]:explicitNormals)normalValues[layout]=add(normalValues[layout],value);
        MorphTarget target{source.name,{}};const auto &positions=channels.at(source.name);
        for(std::size_t r=0;r<d.renderToLayout.size();++r)
        {
            const auto layout=d.renderToLayout[r],point=d.layouts[layout].position;
            const auto p=positions.find(point);const auto n=normalValues.find(layout);
            const auto position=p==positions.end()?Vector3{}:p->second,normal=n==normalValues.end()?Vector3{}:n->second;
            if(!finite(normal))throw std::invalid_argument("Morph normal overflow");
            if(!zero(position)||!zero(normal))
            {
                if(++total>t.maximumRenderMorphDeltas)throw std::length_error("Topology render morph limit");
                target.deltas.push_back({r,position,normal});
            }
        }
        result.geometry.morphTargets.push_back(std::move(target));
    }
    return result;
}
std::vector<std::vector<std::size_t>> topologyAdjacency(const MeshTopologyLayout &layout)
{
    const auto &d=layout.definition();
    std::vector<std::vector<std::size_t>> neighbors(d.positionCount);
    for (const auto &face:d.polygons)
        for (std::size_t i=0;i<face.size();++i)
        {
            const auto a=d.layouts[face[i]].position,b=d.layouts[face[(i+1)%face.size()]].position;
            if (a!=b) { neighbors[a].push_back(b);neighbors[b].push_back(a); }
        }
    for (auto &list:neighbors)
    {
        std::sort(list.begin(),list.end());list.erase(std::unique(list.begin(),list.end()),list.end());
    }
    return neighbors;
}
void reconcileTopologyRender(MeshTopology &t,const SkeletalMeshLod &g,
    std::span<const TextureCoordinate> uv,std::size_t boneCount)
{
    dimensions(t);const auto &d=t.layout->definition();
    const auto triangles=t.layout->triangles();
    if (g.vertices.size()!=d.renderToLayout.size()||g.triangles.size()!=triangles.size()||
        !std::equal(g.triangles.begin(),g.triangles.end(),triangles.begin())||
        uv.size()!=(d.textureCoordinateCount?d.renderToLayout.size():0))
        throw std::invalid_argument("Render topology changed; replace the authoring topology explicitly");
    auto next=t;
    std::vector<std::optional<Vector3>> moved(t.points.size());
    std::vector<std::optional<std::vector<SkinWeight>>> weighted(t.points.size());
    std::vector<std::optional<TextureCoordinate>> mapped(t.textureCoordinates.size());
    std::vector<bool> rendered(t.points.size(),false),renderedLayouts(d.layouts.size(),false);
    for(const auto l:d.renderToLayout)renderedLayouts[l]=true;
    for (std::size_t r=0;r<g.vertices.size();++r)
    {
        const auto v=d.layouts[d.renderToLayout[r]];rendered[v.position]=true;
        const auto &vertex=g.vertices[r];
        if (!finite(vertex.position)) throw std::invalid_argument("Nonfinite topology edit");
        if (!close(vertex.position,t.points[v.position].position))
        {
            auto &old=moved[v.position];
            if (old&&!close(*old,vertex.position)) throw std::invalid_argument("Conflicting seam position edits");
            old=vertex.position;
        }
        auto weights=vertex.weights;
        normalizeSkinWeights(weights,boneCount);
        const auto original=t.points[v.position].weights.empty()?std::vector<SkinWeight>{{0,1}}:t.points[v.position].weights;
        if (!sameWeights(weights,original))
        {
            auto &old=weighted[v.position];
            if (old&&!sameWeights(*old,weights)) throw std::invalid_argument("Conflicting seam weight edits");
            old=std::move(weights);
        }
        if (v.textureCoordinate!=NoTopologyIndex)
        {
            const auto value=uv[r],originalUv=t.textureCoordinates[v.textureCoordinate];
            if (!std::isfinite(value.u)||!std::isfinite(value.v)) throw std::invalid_argument("Nonfinite UV edit");
            if (!close(value.u,originalUv.u)||!close(value.v,originalUv.v))
            {
                auto &old=mapped[v.textureCoordinate];
                if (old&&(!close(old->u,value.u)||!close(old->v,value.v))) throw std::invalid_argument("Conflicting shared UV edits");
                old=value;
            }
        }
    }
    bool changed=false;
    for (std::size_t i=0;i<t.points.size();++i)
    {
        if (moved[i]) { next.points[i].position=*moved[i];changed=true; }
        if (weighted[i]) next.points[i].weights=std::move(*weighted[i]);
    }
    for (std::size_t i=0;i<mapped.size();++i)
        if (mapped[i]) next.textureCoordinates[i]=*mapped[i];
    std::map<std::string,std::vector<std::optional<Vector3>>> desired;
    for (const auto &target:g.morphTargets)
    {
        if (desired.contains(target.name)) throw std::invalid_argument("Duplicate render morph name");
        auto &values=desired[target.name];values.resize(t.points.size());
        std::unordered_set<std::size_t> seen;
        for (const auto &delta:target.deltas)
        {
            if (delta.vertex>=g.vertices.size()||!seen.insert(delta.vertex).second||
                !finite(delta.position)||!finite(delta.normal)) throw std::invalid_argument("Invalid render morph delta");
            if (zero(delta.position)) continue;
            auto &old=values[d.layouts[d.renderToLayout[delta.vertex]].position];
            if (old&&!close(*old,delta.position)) throw std::invalid_argument("Conflicting seam morph edits");
            old=delta.position;
        }
    }
    const auto originals=aggregate(t);
    for (const auto &[name,unused]:originals)
        if (!desired.contains(name)) desired[name].resize(t.points.size());
    for (const auto &[name,values]:desired)
    {
        if(!std::any_of(g.morphTargets.begin(),g.morphTargets.end(),[&](const auto &m){return m.name==name;}))
            for(auto &m:next.morphTargets)if(m.name==name&&m.normalDeltas)
                std::erase_if(*m.normalDeltas,[&](const auto &v){return renderedLayouts[v.layout];});
        auto first=std::find_if(next.morphTargets.begin(),next.morphTargets.end(),[&](const auto &m) { return m.name==name; });
        if (first==next.morphTargets.end())
        { next.morphTargets.push_back({name,{},NoTopologyIndex});first=std::prev(next.morphTargets.end()); }
        PointDeltas others;
        for (auto m=next.morphTargets.begin();m!=next.morphTargets.end();++m)
            if (m!=first&&m->name==name)
                for (const auto &delta:m->deltas) others[delta.point]=add(others[delta.point],delta.position);
        const auto old=originals.find(name);
        first->deltas.clear();
        for (std::size_t p=0;p<t.points.size();++p)
        {
            const auto point=static_cast<std::uint32_t>(p);
            Vector3 total{};
            if (rendered[p]) total=values[p].value_or(Vector3{});
            else if (old!=originals.end())
            { const auto found=old->second.find(point);if (found!=old->second.end()) total=found->second; }
            const auto found=others.find(point);
            const auto delta=found==others.end()?total:sub(total,found->second);
            if (!zero(delta)) first->deltas.push_back({point,delta});
        }
    }
    if (changed) recomputeTopologyNormals(next);
    normalizeMeshTopology(next,boneCount);
    t=std::move(next);
}
} // namespace iiActorObject
