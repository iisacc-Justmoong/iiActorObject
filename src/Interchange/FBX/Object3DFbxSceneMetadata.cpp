#include "Object3DFbxSceneMetadata.h"
#include "Object3DFbxMetadata.h"
#include "Object3DData.h"
#include <algorithm>
#include <stdexcept>

namespace iiActorObject::object3d_detail
{
using fbx_fields::property;
using fbx_fields::string;
using fbx_metadata::number;
using fbx_metadata::scalar;
using fbx_metadata::uniqueChild;

void readFbxSceneMetadata(const ufbx_scene &scene,Object3DData &data,bool restoreAuthoringMetadata,
    std::size_t maxFileBytes,const FbxWarningSink &warn,const FbxRigCatalogDecoder &decodeRigCatalogs)
{
    bool catalogFound=false;
    for(const auto *node:scene.nodes)
    {
        if(const auto *p=property(node->props,"iiObject3DName"))data.name=string(p->value_str);
        else if(const auto *p=property(node->props,"iiCharacterName"))data.name=string(p->value_str);
        if(const auto *p=property(node->props,"iiObject3DKind"))
        {
            if(p->value_int<1||p->value_int>2)throw std::invalid_argument("Unknown FBX 3D object kind");
            data.kind=static_cast<Object3DKind>(p->value_int);
        }
        if(const auto *metadata=uniqueChild(node->element.dom_node,"iiActorObjectRigSamples"))
        {
            if(catalogFound)throw std::invalid_argument("Multiple FBX rig sample metadata blocks");catalogFound=true;
            if(!restoreAuthoringMetadata)warn("Rig sample provenance was ignored with authoring metadata disabled.");
            else
            {
                if(number(uniqueChild(metadata,"Version"))!=1)throw std::invalid_argument("Unknown FBX rig sample metadata version");
                const auto &raw=scalar(uniqueChild(metadata,"Data"));const auto blob=raw.value_blob;
                const auto maximum=std::min<std::size_t>(maxFileBytes,64*1024*1024);
                if(raw.type!=UFBX_DOM_VALUE_STRING||blob.size>maximum||(!blob.data&&blob.size))
                    throw std::invalid_argument("Invalid FBX rig sample metadata");
                data.rigSampleCatalogs=decodeRigCatalogs({static_cast<const std::uint8_t*>(blob.data),blob.size},maximum);
            }
        }
    }
}
}
