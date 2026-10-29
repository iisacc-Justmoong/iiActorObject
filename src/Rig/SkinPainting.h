#pragma once
#include <iiActorObject/MeshEditing.h>

namespace iiActorObject
{
enum class SkinPaintMode {Replace,Add,Subtract,Smooth};
struct SkinPaintOptions final
{
    SkinPaintMode mode=SkinPaintMode::Replace;
    double value=1;
    MeshBrushSettings brush;
    std::vector<BoneIndex> lockedBones;
    std::size_t maximumInfluences=0; // Zero retains every positive influence.
    BoneIndex fallbackBone=NoBone;
};
class IIACTOROBJECT_EXPORT SkinPainter final
{
  public:
    // Smooth blends the complete influence row and requires bone==NoBone.
    // Other modes edit one bone and redistribute unlocked residual weight.
    [[nodiscard]] static BrushStrokeResult paint(Object3DData &data,std::size_t mesh,BoneIndex bone,
        std::span<const MeshBrushSample> samples,const SkinPaintOptions &options={});
};
} // namespace iiActorObject
