#include "Object3DEditorInstanceTransfer.h"
#include <stdexcept>

namespace iiActorObject
{
Object3DInstanceTransferResult transferObject3DEditorInstance(const Object3DHandle &source,const iiCharacter &actor,
    Object3DEditorRigSet &rigs,const Object3DInstanceTransferOptions &options)
{
    if(rigs.boundObject()!=source)throw std::invalid_argument("Instance source must match the rig set's last bound object");
    const auto target=rigs.editor().get().object();
    auto result=transferObject3DInstance(source,actor,target,options);
    result.report.rigsRebound=rigs.synchronize();return result;
}
} // namespace iiActorObject
