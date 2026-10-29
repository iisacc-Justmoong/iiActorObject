#include "Object3DFbxSkinWrite.h"
#include "Object3DFbxSceneNodes.h"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace iiActorObject::object3d_detail
{
using namespace fbx_binary;
using namespace fbx_scene_nodes;
namespace
{
template<class Point>
void write(std::string_view name,const SkinningProfile &profile,std::span<const Point> points,
    std::span<const FbxSkinBone> bones,std::int64_t skinId,std::int64_t geometryId,std::int64_t &next,
    Node &objects,Node &connections)
{
    if(points.size()>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        throw std::length_error("FBX skin point index limit");
    validateSkinningProfile(profile,points.size());
    for(const auto &point:points)for(const auto influence:point.weights)
        if(influence.bone>=bones.size()||!std::isfinite(influence.weight)||influence.weight<0)
            throw std::invalid_argument("Invalid FBX skin influence");
    const auto connect=[&](std::int64_t child,std::int64_t parent)
    {connections.children.push_back({"C",{prop("OO"),prop(child),prop(parent)}, {}});};
    const char *method=profile.method==SkinningMethod::DualQuaternion?"DualQuaternion":
                       profile.method==SkinningMethod::Blended?"Blend":"Linear";
    objects.children.push_back({"Deformer",{prop(skinId),prop(objectName(std::string(name)+"Skin","Deformer")),prop("Skin")},
        {value("Version",prop(101)),value("Link_DeformAcuracy",prop(50.0)),value("SkinningType",prop(method))}});
    if(profile.method==SkinningMethod::Blended)
    {
        std::vector<std::int32_t> indices;indices.reserve(profile.blendWeights.size());
        for(std::size_t i=0;i<profile.blendWeights.size();++i)indices.push_back(static_cast<std::int32_t>(i));
        objects.children.back().children.push_back(value("Indexes",array(indices)));
        objects.children.back().children.push_back(value("BlendWeights",array(profile.blendWeights)));
    }
    connect(skinId,geometryId);
    for(std::size_t bone=0;bone<bones.size();++bone)
    {
        std::vector<std::int32_t> indices;std::vector<double> weights;
        for(std::size_t i=0;i<points.size();++i)
            for(const auto influence:points[i].weights)
                if(influence.bone==bone&&influence.weight>0)
                {indices.push_back(static_cast<std::int32_t>(i));weights.push_back(influence.weight);}
        if(indices.empty())continue;
        if(next==std::numeric_limits<std::int64_t>::max())throw std::overflow_error("FBX skin cluster ID range");
        const auto cluster=next++;
        objects.children.push_back({"Deformer",{prop(cluster),prop(objectName(std::string(bones[bone].name)+"Cluster","SubDeformer")),prop("Cluster")},
            {value("Version",prop(100)),{"UserData",{prop(""),prop("")},{}},
             value("Indexes",array(indices)),value("Weights",array(weights)),
             value("Transform",array(bones[bone].inverseBind.elements())),
             value("TransformLink",array(bones[bone].bindWorld.elements()))}});
        connect(cluster,skinId);connect(bones[bone].nodeId,cluster);
    }
}
}
void writeFbxSkin(std::string_view name,const SkinningProfile &profile,std::span<const TopologyControlPoint> points,
    std::span<const FbxSkinBone> bones,std::int64_t skinId,std::int64_t geometryId,std::int64_t &next,
    Node &objects,Node &connections)
{write(name,profile,points,bones,skinId,geometryId,next,objects,connections);}
void writeFbxSkin(std::string_view name,const SkinningProfile &profile,std::span<const SkinVertex> vertices,
    std::span<const FbxSkinBone> bones,std::int64_t skinId,std::int64_t geometryId,std::int64_t &next,
    Node &objects,Node &connections)
{write(name,profile,vertices,bones,skinId,geometryId,next,objects,connections);}
} // namespace iiActorObject::object3d_detail
