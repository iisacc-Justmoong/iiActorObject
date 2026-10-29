#pragma once
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DRig.h>
#include <iiActorObject/Object3DJointIdentity.h>

namespace iiActorObject
{
struct Object3DEditorRigOptions final
{
    EditedRigJointIdentity jointIdentity=EditedRigJointIdentity::RequireEvidence;
};

// A non-owning view of an editor and several engine-owned rig sessions.
// One host thread mutates and evaluates the view and editor.
class IIACTOROBJECT_EXPORT Object3DEditorRigSet final
{
  public:
    Object3DEditorRigSet(Object3DEditorHandle editor,std::span<const Object3DRigHandle> rigs,
        Object3DEditorRigOptions options={});
    [[nodiscard]] const Object3DEditorHandle &editor() const noexcept;
    // The last successfully bound snapshot; does not synchronize.
    [[nodiscard]] Object3DHandle boundObject() const;
    // Rebind all sessions to the current editor snapshot in one transaction.
    // Returns false when the active object has not changed.
    [[nodiscard]] bool synchronize();
    // Synchronize on access. Spans expire at the next successful synchronization.
    [[nodiscard]] std::span<const Object3DRigHandle> rigs();
    [[nodiscard]] Object3DRigHandle rig(std::size_t index);
    [[nodiscard]] Object3DRigEvaluation evaluate(std::size_t index,std::span<const double> controls,
        RigControlSpace space=RigControlSpace::Raw,std::size_t lod=0);
    [[nodiscard]] Object3DRigEvaluation evaluateNamed(std::size_t index,std::span<const NamedRigControl> controls,
        RigControlSpace space=RigControlSpace::Raw,std::size_t lod=0);
  private:
    Object3DEditorHandle editor_;
    std::vector<Object3DRigHandle> rigs_;
    Object3DEditorRigOptions options_;
};
}
