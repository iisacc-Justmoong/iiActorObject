#pragma once
#include <iiActorObject/MeshScreenSelection.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DObj.h>
#include <array>

class InstalledScreenDepth final:public iiActorObject::MeshScreenDepthSampler
{
  public:
    std::optional<double> sample(iiActorObject::MeshScreenPoint)const override{return .5;}
};
inline int meshScreenSelectionConsumer()
{
    using namespace iiActorObject;
    Object3DEngine engine;const auto asset=engine.createObject(parseObject3DObj(
        "v -.5 -.5 0\nv .5 -.5 0\nv 0 .5 0\nf 1 2 3\n"));
    auto actor=asset.get().instantiate();SkeletalMeshComponent *component=nullptr;
    for(auto *part:actor->components())if(auto *mesh=dynamic_cast<SkeletalMeshComponent*>(part)){component=mesh;break;}
    if(!component)return 1;
    const auto captured=captureMeshScreenPositions(asset.get().data().meshes[0],component->deformedVertices());
    const std::array positions{MeshScreenMeshPositions{0,captured.positions,captured.available}};
    InstalledScreenDepth depth;MeshScreenSelectionOptions options;options.positions=positions;options.depthSampler=&depth;
    MeshScreenView view;view.viewport={0,0,100,100,true};
    const auto result=selectMeshPointsOnScreen(asset.get(),view,MeshScreenCircle{{25,75},2},options);
    if(result.selectedPoints!=1||result.masks[0].weights!=std::vector<double>({1,0,0}))return 2;
    auto editor=engine.editObject(asset);editor.get().setPointSelection("selected",result.masks);
    if(editor.get().pointSelection("selected")[0]!=result.masks[0])return 3;
    return 0;
}
