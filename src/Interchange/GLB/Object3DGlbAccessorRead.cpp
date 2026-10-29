#include "Object3DGlbAccessorRead.h"
#include "Object3DGlbJson.h"
#include <limits>
#include <stdexcept>

namespace iiActorObject::object3d_detail::glb_import
{
namespace
{
void dimensions(const GlbDocument &document,std::size_t index,GlbAccessorType shape,std::size_t count)
{const auto info=document.accessor(index);if(info.type!=shape||info.count!=count)fail("accessor dimensions do not match mesh");}
bool unsignedComponent(GlbComponentType type)
{return type==GlbComponentType::UnsignedByte||type==GlbComponentType::UnsignedShort||type==GlbComponentType::UnsignedInt;}
}
void validateGlbFloatAttribute(const GlbDocument &document,std::size_t index,GlbAccessorType shape,
    std::size_t count,bool quantized,GlbFloatRole role)
{
    dimensions(document,index,shape,count);const auto info=document.accessor(index);
    if(info.component==GlbComponentType::Float&&!info.normalized)return;
    const bool signedInteger=info.component==GlbComponentType::Byte||info.component==GlbComponentType::Short;
    const bool unsignedInteger=info.component==GlbComponentType::UnsignedByte||info.component==GlbComponentType::UnsignedShort;
    if(quantized)
    {
        if(role==GlbFloatRole::Position&&(signedInteger||unsignedInteger))return;
        if(role==GlbFloatRole::MorphPosition&&signedInteger)return;
        if((role==GlbFloatRole::Normal||role==GlbFloatRole::MorphNormal||role==GlbFloatRole::Tangent||role==GlbFloatRole::MorphTangent)&&signedInteger&&info.normalized)return;
    }
    fail("invalid component type for mesh or morph attribute");
}
std::vector<double> readGlbFloatAttribute(const GlbDocument &document,std::size_t index,GlbAccessorType shape,
    std::size_t count,bool quantized,GlbFloatRole role)
{validateGlbFloatAttribute(document,index,shape,count,quantized,role);return document.readFloats(index);}
std::vector<double> readGlbTexcoords(const GlbDocument &document,std::size_t index,std::size_t count,bool quantized)
{
    dimensions(document,index,GlbAccessorType::Vec2,count);const auto info=document.accessor(index);
    const bool standard=info.component==GlbComponentType::Float&&!info.normalized;
    const bool unorm=(info.component==GlbComponentType::UnsignedByte||info.component==GlbComponentType::UnsignedShort)&&info.normalized;
    const bool quantInteger=info.component==GlbComponentType::Byte||info.component==GlbComponentType::Short||
                            info.component==GlbComponentType::UnsignedByte||info.component==GlbComponentType::UnsignedShort;
    if(!standard&&!unorm&&!(quantized&&quantInteger))fail("unsupported TEXCOORD component type");
    return document.readFloats(index);
}
GlbSkinAttributeValues readGlbSkinAttributes(const GlbDocument &document,std::span<const std::size_t> joints,
    std::span<const std::size_t> weights,std::size_t count,bool skinned)
{
    if(skinned&&joints.empty())fail("skinned mesh is missing joint weights");
    if(!skinned&&!joints.empty())fail("joint weights require a skin");
    if(joints.size()!=weights.size())fail("joint and weight set counts differ");
    GlbSkinAttributeValues result;result.joints.reserve(joints.size());result.weights.reserve(weights.size());
    for(std::size_t set=0;set<joints.size();++set)
    {
        dimensions(document,joints[set],GlbAccessorType::Vec4,count);const auto jointInfo=document.accessor(joints[set]);
        if(jointInfo.normalized||(jointInfo.component!=GlbComponentType::UnsignedByte&&jointInfo.component!=GlbComponentType::UnsignedShort))fail("invalid JOINTS component type");
        dimensions(document,weights[set],GlbAccessorType::Vec4,count);const auto weightInfo=document.accessor(weights[set]);
        const bool f=weightInfo.component==GlbComponentType::Float&&!weightInfo.normalized;
        const bool unorm=(weightInfo.component==GlbComponentType::UnsignedByte||weightInfo.component==GlbComponentType::UnsignedShort)&&weightInfo.normalized;
        if(!f&&!unorm)fail("invalid WEIGHTS component type");
        result.joints.push_back(document.readUnsigned(joints[set]));result.weights.push_back(document.readFloats(weights[set]));
    }
    return result;
}
GlbPointIdValues readGlbPointIds(const GlbDocument &document,std::size_t index,std::size_t count)
{
    GlbPointIdValues result;
    try
    {
        const auto info=document.accessor(index);
        if(info.type!=GlbAccessorType::Vec2||info.count!=count||info.normalized||info.component!=GlbComponentType::UnsignedShort)
            result.invalid=true;
        else result.values=document.readUnsigned(index);
    }
    catch(const std::invalid_argument&){result.invalid=true;}
    catch(const std::out_of_range&){result.invalid=true;}
    return result;
}
std::vector<std::uint32_t> readGlbTriangleIndices(const GlbDocument &document,std::optional<std::size_t> index,
    std::size_t vertices,std::size_t maximumTriangles,std::size_t usedTriangles)
{
    std::vector<std::uint32_t> result;
    if(index)
    {
        const auto info=document.accessor(*index);
        if(info.type!=GlbAccessorType::Scalar||info.normalized||!unsignedComponent(info.component))fail("invalid triangle indices accessor");
        if(!info.count||info.count%3||usedTriangles>maximumTriangles||info.count/3>maximumTriangles-usedTriangles)
            fail("triangle limit or invalid index count");
        result=document.readUnsigned(*index);
    }
    else
    {
        if(!vertices||vertices%3||usedTriangles>maximumTriangles||vertices/3>maximumTriangles-usedTriangles)
            fail("triangle limit or invalid vertex count");
        if(vertices>std::numeric_limits<std::uint32_t>::max())fail("vertex index limit");
        result.reserve(vertices);for(std::size_t i=0;i<vertices;++i)result.push_back(static_cast<std::uint32_t>(i));
    }
    return result;
}
}
