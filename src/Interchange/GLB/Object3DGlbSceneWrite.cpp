#include "Object3DGlbSceneWrite.h"
#include "Object3DGlbBuffer.h"
#include "Object3DData.h"
#include <ostream>

namespace iiActorObject::object3d_detail::glb_export
{
void writeGlbScene(std::ostream &out,std::string_view name,std::span<const Bone> bones,
    std::span<const Object3DMesh> meshes,std::span<const Object3DJointSemantic> semantics,
    std::string_view rigSamplesJson,bool hasRig,std::size_t inverseBindAccessor,
    std::optional<std::string_view> externalDocument)
{
    const auto boneCount=hasRig?bones.size():0;
    out<<"{\"asset\":{\"version\":\"2.0\",\"generator\":\"iiActorObject Object3DEngine\"";
    if(externalDocument)out<<",\"extras\":{\"iiActorObjectExternalVersion\":1,\"iiActorObjectExternalDocument\":"
                           <<quote(*externalDocument)<<'}';
    out<<"},\"scene\":0,\"scenes\":[{\"nodes\":["<<boneCount<<"]}],\"nodes\":[";
    for(std::size_t i=0;i<boneCount;++i)
    {
        if(i)out<<',';
        const auto &bone=bones[i];const auto &t=bone.bindTransform;
        out<<"{\"name\":"<<quote(bone.name)<<",\"translation\":["<<t.position.x<<','
           <<t.position.y<<','<<t.position.z<<"],\"rotation\":["<<t.rotation.x<<','
           <<t.rotation.y<<','<<t.rotation.z<<','<<t.rotation.w<<"],\"scale\":["<<t.scale.x
           <<','<<t.scale.y<<','<<t.scale.z<<']';
        if(externalDocument)out<<",\"extras\":{\"iiActorObjectSourceBone\":"<<i<<'}';
        bool first=true;
        for(std::size_t c=0;c<bones.size();++c)
            if(bones[c].parent==i){out<<(first?",\"children\":[":",")<<c;first=false;}
        if(!first)out<<']';
        out<<'}';
    }
    if(boneCount)out<<',';
    out<<"{\"name\":"<<quote(name)<<",\"children\":[";
    bool firstChild=true;
    if(boneCount){out<<'0';firstChild=false;}
    for(std::size_t i=0;i<meshes.size();++i)
    {
        if(!firstChild)out<<',';
        out<<boneCount+1+i;firstChild=false;
    }
    out<<"],\"extras\":{\"iiObject3D\":{\"schemaVersion\":1,\"semantics\":{";
    for(std::size_t i=0;i<semantics.size();++i)
    {
        if(i)out<<',';
        out<<quote(semantics[i].name)<<':'<<semantics[i].bone;
    }
    out<<'}';
    if(!rigSamplesJson.empty())out<<",\"rigSamples\":"<<rigSamplesJson;
    out<<"}}}";
    for(std::size_t i=0;i<meshes.size();++i)
    {
        out<<",{\"name\":"<<quote(meshes[i].name)<<",\"mesh\":"<<i;
        if(hasRig)out<<",\"skin\":0";
        out<<'}';
    }
    out<<']';
    if(hasRig)
    {
        out<<",\"skins\":[{\"inverseBindMatrices\":"<<inverseBindAccessor<<",\"skeleton\":0,\"joints\":[";
        for(std::size_t i=0;i<bones.size();++i){if(i)out<<',';out<<i;}
        out<<"]}]";
    }
}
} // namespace iiActorObject::object3d_detail::glb_export
