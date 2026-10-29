#include <iiActorObject/MeshPointMask.h>
#include <iiActorObject/Object3DEditLayerData.h>
#include <iiActorObject/Object3DPoseProgramData.h>
#include <iiActorObject/Object3DProjectOptions.h>
#include <iiActorObject/Object3DTemplateMorphData.h>
#include <iiActorObject/Object3DTemplateTypes.h>
#include <iiActorObject/SurfaceFitSettings.h>
#include <array>

int main()
{
    using namespace iiActorObject;
    Object3DProjectOptions options;Object3DEditLayerInfo layer;
    Object3DPoseProgram pose;TemplateValue value{"width",.5};TemplateMorphRigProgram morph;
    MeshPointMask mask;SurfaceFitSettings settings;
    if(options.maximumAssets!=4096||layer.evaluation!=Object3DLayerEvaluation::Deferred||
       !pose.joints.empty()||value.value!=.5||!morph.channels.empty()||mask.mesh!=0||
       settings.options().mobility.size()!=0)return 1;
}
