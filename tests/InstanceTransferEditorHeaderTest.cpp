#include <iiActorObject/Object3DEditorInstanceTransfer.h>

int instanceTransferEditorHeaderProbe()
{
    auto *transfer = &iiActorObject::transferObject3DEditorInstance;
    return transfer ? 0 : 1;
}
