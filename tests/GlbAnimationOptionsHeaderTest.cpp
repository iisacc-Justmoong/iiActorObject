#include <iiActorObject/Object3DGlbAnimationOptions.h>

int main()
{
    iiActorObject::GlbAnimationExportOptions animation;
    iiActorObject::GlbMorphAnimationOptions morph;
    return animation.maximumKeys==1'000'000&&morph.bindings.empty()&&morph.indexedBindings.empty()?0:1;
}
