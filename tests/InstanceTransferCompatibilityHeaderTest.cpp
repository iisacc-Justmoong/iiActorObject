#include <iiActorObject/Object3DInstanceTransfer.h>
#include <type_traits>

// The original header retains its editor and rig declarations.
static_assert(std::is_class_v<iiActorObject::Object3DEditorRigSet>);
static_assert(static_cast<int>(iiActorObject::EditedRigJointIdentity::RequireEvidence)==0);
int instanceTransferCoreHeaderProbe();
int instanceTransferEditorHeaderProbe();
int main()
{
    return instanceTransferCoreHeaderProbe() || instanceTransferEditorHeaderProbe();
}
