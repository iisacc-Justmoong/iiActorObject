#include "Object3DGlbDocumentWrite.h"
#include "Object3DGlbBuffer.h"
#include "Object3DGlbMaterialWrite.h"
#include "Object3DGlbSceneWrite.h"
#include "Object3DImage.h"
#include "Object3DMaterial.h"
#include "Object3DMesh.h"
#include <stdexcept>

namespace iiActorObject::object3d_detail::glb_export
{
std::string writeGlbDocumentJson(const Buffer &buffer,const GlbDocumentWriteInput &input)
{
    if(input.meshJson.size()!=input.meshes.size())throw std::invalid_argument("GLB mesh JSON count mismatch");
    if(input.externalDocument&&input.materialVariants.size()!=input.materials.size())
        throw std::invalid_argument("GLB external edits cannot identify UV-specific material variants");
    auto json=stream();
    writeGlbScene(json,input.objectName,input.bones,input.meshes,input.semantics,input.rigSamplesJson,
                  input.hasRig,input.inverseBindAccessor,input.externalDocument);
    json<<",\"meshes\":";jsonArray(json,input.meshJson);
    writeGlbMaterials(json,input.materials,input.materialVariants,input.images,input.imageViews,
                      input.externalDocument.has_value());
    if(input.animationJson)json<<",\"animations\":["<<*input.animationJson<<']';
    json<<",\"bufferViews\":";jsonArray(json,buffer.views);
    json<<",\"accessors\":";jsonArray(json,buffer.accessors);
    json<<",\"buffers\":[{\"byteLength\":"<<buffer.bytes.size()<<"}]}";
    return json.str();
}
} // namespace iiActorObject::object3d_detail::glb_export
