#include "SurfaceFitSettings.h"
#include <stdexcept>
#include <tuple>

namespace iiActorObject
{
namespace
{
auto parameters(const SurfaceFitOptions &v)
{
    return std::tie(v.clearance,v.tolerance,v.relaxation,v.edgeStiffness,v.maximumDisplacement,v.iterations,
        v.includeUnusedPoints,v.checkTriangles,v.allowUnresolved,v.maximumPoints,v.maximumPointVisits,
        v.maximumTriangles,v.maximumNodeVisits,v.maximumTriangleTests,v.escapeConcavities,v.fairing);
}
auto colliderParameters(const MeshColliderOptions &v)
{return std::tie(v.weldRenderSeams,v.minimumRelativeVolume,v.maximumPoints,v.maximumTriangles,v.maximumComponents,v.maximumPreprocessVisits);}
}
SurfaceFitSettings::SurfaceFitSettings(const SurfaceFitOptions &options):parameters_(options)
{
    parameters_.mobility={};if(options.mobility.size()>options.maximumPoints)throw std::length_error("Owned surface mobility budget");
    mobility_.assign(options.mobility.begin(),options.mobility.end());
}
SurfaceFitOptions SurfaceFitSettings::options() const &{auto result=parameters_;result.mobility=mobility_;return result;}
bool SurfaceFitSettings::operator==(const SurfaceFitSettings &other) const{return parameters(parameters_)==parameters(other.parameters_)&&mobility_==other.mobility_;}
bool SurfaceFittingRule::operator==(const SurfaceFittingRule &other) const
{return targetMesh==other.targetMesh&&obstacleMeshes==other.obstacleMeshes&&fitting==other.fitting&&colliderParameters(collider)==colliderParameters(other.collider)&&alternateQuadDiagonals==other.alternateQuadDiagonals&&enabled==other.enabled;}
} // namespace iiActorObject
