#include "MeshScreenSelection.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <type_traits>

namespace iiActorObject
{
MeshScreenDepthSampler::~MeshScreenDepthSampler()=default;
namespace
{
bool finite(MeshScreenPoint p){return std::isfinite(p.x)&&std::isfinite(p.y);}
bool finite(Vector3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
double checked(double value)
{
    if(!std::isfinite(value))throw std::overflow_error("Screen selection arithmetic overflow");
    return value;
}
void validate(const MeshScreenView &view,const MeshScreenRegion &region,const MeshScreenSelectionOptions &options)
{
    const auto &v=view.viewport;
    if(!std::all_of(view.clipFromObject.begin(),view.clipFromObject.end(),[](double n){return std::isfinite(n);})||
       !std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.width)||!std::isfinite(v.height)||
       v.width<=0||v.height<=0||!std::isfinite(v.x+v.width)||!std::isfinite(v.y+v.height)||
       (view.depthRange!=MeshScreenDepthRange::MinusOneToOne&&view.depthRange!=MeshScreenDepthRange::ZeroToOne)||
       (options.missingDepth!=MeshScreenMissingDepth::Reject&&options.missingDepth!=MeshScreenMissingDepth::Exclude&&
        options.missingDepth!=MeshScreenMissingDepth::Include)||
       !std::isfinite(options.edgeTolerance)||options.edgeTolerance<0||
       !std::isfinite(options.depthTolerance)||options.depthTolerance<0||options.depthTolerance>1||
       !options.maximumPoints||!options.maximumTriangleIndices||!options.maximumPolygonEdgeTests)
        throw std::invalid_argument("Invalid screen selection view or limits");
    std::visit([](const auto &shape)
    {
        using Shape=std::decay_t<decltype(shape)>;
        if constexpr(std::is_same_v<Shape,MeshScreenRectangle>)
        {
            if(!finite(shape.first)||!finite(shape.last))throw std::invalid_argument("Invalid screen rectangle");
        }
        else if constexpr(std::is_same_v<Shape,MeshScreenCircle>)
        {
            if(!finite(shape.center)||!std::isfinite(shape.radius)||shape.radius<0)
                throw std::invalid_argument("Invalid screen circle");
        }
        else
        {
            if(shape.points.size()<3||shape.points.size()>4096||
               !std::all_of(shape.points.begin(),shape.points.end(),[](auto point){return finite(point);}))
                throw std::invalid_argument("Invalid screen polygon");
        }
    },region);
}
struct Projected final {MeshScreenPoint screen;double depth=0;};
std::optional<Projected> project(Vector3 p,const MeshScreenView &view)
{
    if(!finite(p))throw std::invalid_argument("Nonfinite screen selection point");
    std::array<double,4> clip{};const auto &m=view.clipFromObject;
    for(std::size_t row=0;row<4;++row)
        clip[row]=checked(m[row]*p.x+m[4+row]*p.y+m[8+row]*p.z+m[12+row]);
    const auto w=clip[3];if(w<=0)return {};
    if(clip[0]<-w||clip[0]>w||clip[1]<-w||clip[1]>w||clip[2]>w||
       (view.depthRange==MeshScreenDepthRange::MinusOneToOne?clip[2]<-w:clip[2]<0))return {};
    const auto x=clip[0]/w,y=clip[1]/w,z=clip[2]/w;const auto &v=view.viewport;
    const auto screenX=checked(v.x+(.5+.5*x)*v.width);
    const auto screenY=checked(v.y+(.5+(v.yDown?-.5:.5)*y)*v.height);
    const auto depth=view.depthRange==MeshScreenDepthRange::MinusOneToOne?.5+.5*z:z;
    return Projected{{screenX,screenY},std::clamp(depth,0.,1.)};
}
bool polygonContains(MeshScreenPoint p,std::span<const MeshScreenPoint> points,double tolerance,
                     MeshScreenSelectionResult &result,const MeshScreenSelectionOptions &options)
{
    bool inside=false;
    for(std::size_t i=0;i<points.size();++i)
    {
        if(result.polygonEdgeTests==options.maximumPolygonEdgeTests)
            throw std::length_error("Screen polygon edge budget");
        ++result.polygonEdgeTests;const auto a=points[i],b=points[(i+1)%points.size()];
        const auto dx=checked(b.x-a.x),dy=checked(b.y-a.y),length=checked(std::hypot(dx,dy));
        const auto px=checked(p.x-a.x),py=checked(p.y-a.y);
        if(length==0)
        {
            if(checked(std::hypot(px,py))<=tolerance)return true;
        }
        else
        {
            const auto ux=dx/length,uy=dy/length;
            const auto along=std::clamp(checked(px*ux+py*uy),0.,length);
            if(checked(std::hypot(checked(px-ux*along),checked(py-uy*along)))<=tolerance)return true;
        }
        if((a.y>p.y)!=(b.y>p.y))
        {
            const auto fraction=checked((p.y-a.y)/checked(b.y-a.y));
            const auto intersection=checked(a.x*(1-fraction)+b.x*fraction);
            if(p.x<intersection)inside=!inside;
        }
    }
    return inside;
}
bool contains(MeshScreenPoint p,const MeshScreenRegion &region,MeshScreenSelectionResult &result,
              const MeshScreenSelectionOptions &options)
{
    return std::visit([&](const auto &shape)
    {
        using Shape=std::decay_t<decltype(shape)>;const auto tolerance=options.edgeTolerance;
        if constexpr(std::is_same_v<Shape,MeshScreenRectangle>)
            return p.x>=checked(std::min(shape.first.x,shape.last.x)-tolerance)&&
                   p.x<=checked(std::max(shape.first.x,shape.last.x)+tolerance)&&
                   p.y>=checked(std::min(shape.first.y,shape.last.y)-tolerance)&&
                   p.y<=checked(std::max(shape.first.y,shape.last.y)+tolerance);
        else if constexpr(std::is_same_v<Shape,MeshScreenCircle>)
            return checked(std::hypot(checked(p.x-shape.center.x),checked(p.y-shape.center.y)))<=
                   checked(shape.radius+tolerance);
        else return polygonContains(p,shape.points,tolerance,result,options);
    },region);
}
std::size_t pointCount(const Object3DMesh &mesh)
{return mesh.topology?mesh.topology->points.size():mesh.geometry.vertices.size();}
}
MeshScreenPositionSnapshot captureMeshScreenPositions(const Object3DMesh &mesh,
    std::span<const DeformedVertex> vertices,const MeshScreenCaptureOptions &options)
{
    const auto count=pointCount(mesh);
    if(!std::isfinite(options.seamTolerance)||options.seamTolerance<0||!options.maximumPoints||
       !options.maximumRenderVertices)throw std::invalid_argument("Invalid screen position capture options");
    if(count>options.maximumPoints||vertices.size()>options.maximumRenderVertices)
        throw std::length_error("Screen position capture budget");
    if(vertices.size()!=mesh.geometry.vertices.size())throw std::invalid_argument("Screen render vertex count mismatch");
    if(mesh.topology&&(!mesh.topology->layout||
        mesh.topology->layout->definition().renderToLayout.size()!=vertices.size()))
        throw std::invalid_argument("Invalid screen render topology");
    MeshScreenPositionSnapshot result;result.positions.resize(count);result.available.resize(count);
    for(std::size_t vertex=0;vertex<vertices.size();++vertex)
    {
        const auto position=vertices[vertex].position;if(!finite(position))throw std::invalid_argument("Invalid deformed screen position");
        auto point=vertex;
        if(mesh.topology)
        {
            const auto &definition=mesh.topology->layout->definition();
            point=definition.layouts.at(definition.renderToLayout[vertex]).position;
        }
        auto &value=result.positions.at(point);auto &available=result.available.at(point);
        if(available)
        {
            const auto distance=checked(std::hypot(checked(value.x-position.x),checked(value.y-position.y),
                                                    checked(value.z-position.z)));
            if(distance>options.seamTolerance)throw std::invalid_argument("Deformed screen seam positions disagree");
        }
        else {value=position;available=1;}
    }
    return result;
}
MeshScreenSelectionResult selectMeshPointsOnScreen(const Object3DAsset &asset,const MeshScreenView &view,
    const MeshScreenRegion &region,const MeshScreenSelectionOptions &options)
{
    validate(view,region,options);const auto &data=asset.data();std::vector<std::size_t> selected;
    if(options.meshes.size()>data.meshes.size())throw std::invalid_argument("Invalid screen selection mesh count");
    if(options.meshes.empty())for(std::size_t i=0;i<data.meshes.size();++i)selected.push_back(i);
    else selected.assign(options.meshes.begin(),options.meshes.end());
    std::sort(selected.begin(),selected.end());
    if(std::adjacent_find(selected.begin(),selected.end())!=selected.end())
        throw std::invalid_argument("Duplicate screen selection mesh");
    std::size_t totalPoints=0,totalIndices=0;
    for(const auto index:selected)
    {
        const auto &mesh=data.meshes.at(index);const auto count=pointCount(mesh);
        if(count>options.maximumPoints-totalPoints)throw std::length_error("Screen selection point budget");
        totalPoints+=count;
        if(!options.includeUnusedPoints)
        {
            const auto countIndices=mesh.geometry.triangles.size();
            if(countIndices>options.maximumTriangleIndices-totalIndices)throw std::length_error("Screen selection index budget");
            totalIndices+=countIndices;
        }
    }
    if(options.positions.size()>selected.size())throw std::invalid_argument("Invalid screen selection override count");
    std::map<std::size_t,const MeshScreenMeshPositions*> overrides;
    for(const auto &entry:options.positions)
    {
        const auto &mesh=data.meshes.at(entry.mesh);
        if(!std::binary_search(selected.begin(),selected.end(),entry.mesh)||
           entry.positions.size()!=pointCount(mesh)||!overrides.emplace(entry.mesh,&entry).second||
           (!entry.available.empty()&&entry.available.size()!=entry.positions.size())||
           !std::all_of(entry.positions.begin(),entry.positions.end(),[](auto p){return finite(p);})||
           !std::all_of(entry.available.begin(),entry.available.end(),[](auto value){return value<=1;}))
            throw std::invalid_argument("Invalid screen selection position override");
    }
    MeshScreenSelectionResult result;result.masks.reserve(selected.size());
    for(const auto index:selected)
    {
        const auto &mesh=data.meshes[index];const auto count=pointCount(mesh);
        std::vector<bool> visible(count,options.includeUnusedPoints);
        if(!options.includeUnusedPoints)for(const auto vertex:mesh.geometry.triangles)
        {
            std::size_t point=vertex;
            if(mesh.topology)
            {
                const auto &definition=mesh.topology->layout->definition();
                point=definition.layouts.at(definition.renderToLayout.at(vertex)).position;
            }
            visible.at(point)=true;
        }
        MeshPointMask mask{index,std::vector<double>(count)};const auto override=overrides.find(index);
        for(std::size_t point=0;point<count;++point)
        {
            if(!visible[point]){++result.skippedUnusedPoints;continue;}
            if(override!=overrides.end()&&!override->second->available.empty()&&!override->second->available[point])
            {++result.unavailablePoints;continue;}
            const auto value=override!=overrides.end()?override->second->positions[point]:
                (mesh.topology?mesh.topology->points[point].position:mesh.geometry.vertices[point].position);
            const auto projected=project(value,view);if(!projected){++result.clippedPoints;continue;}
            if(!contains(projected->screen,region,result,options)){++result.outsideRegionPoints;continue;}
            if(options.depthSampler)
            {
                ++result.depthQueries;const auto depth=options.depthSampler->sample(projected->screen);
                if(!depth)
                {
                    ++result.missingDepthPoints;
                    if(options.missingDepth==MeshScreenMissingDepth::Reject)
                        throw std::invalid_argument("Screen selection depth is missing");
                    if(options.missingDepth==MeshScreenMissingDepth::Exclude)continue;
                }
                else
                {
                    if(!std::isfinite(*depth)||*depth<0||*depth>1)throw std::invalid_argument("Invalid screen selection depth");
                    const auto hidden=view.reversedDepth?projected->depth+options.depthTolerance<*depth:
                                                           projected->depth-options.depthTolerance>*depth;
                    if(hidden){++result.occludedPoints;continue;}
                }
            }
            mask.weights[point]=1;++result.selectedPoints;
        }
        result.masks.push_back(std::move(mask));
    }
    return result;
}
} // namespace iiActorObject
