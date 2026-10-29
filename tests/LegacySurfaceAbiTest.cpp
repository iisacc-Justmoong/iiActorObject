// Frozen declarations from the prior public header. This translation unit must
// not include the current SurfaceBinding.h: it emulates an existing binary caller.
#include <iiActorObject/Object3DObj.h>
#include <array>
#include <limits>
#include <optional>
#include <iostream>

namespace iiActorObject
{
struct SurfaceBindingPoint final
{
    std::uint32_t point=0,triangle=0;
    std::array<std::uint32_t,3> vertices{};
    std::array<double,3> barycentric{};
    Vector3 frameOffset;
};
enum class SurfaceBindingMode {TriangleFrame,ControlPoints};
struct SurfaceBindingAxisScale final {std::uint32_t a=0,b=0;double referenceSpan=1;};
struct SurfaceBinding final
{
    std::size_t sourceMesh=0,targetMesh=0,sourceVertexCount=0,targetPointCount=0;
    std::vector<SurfaceBindingPoint> points;
    SurfaceBindingMode mode=SurfaceBindingMode::TriangleFrame;
    std::array<std::optional<SurfaceBindingAxisScale>,3> offsetScales{};
};
struct SurfaceBindingOptions final
{
    double maximumDistance=std::numeric_limits<double>::infinity();
    bool includeUnusedPoints=false;
};
[[nodiscard]] IIACTOROBJECT_EXPORT SurfaceBinding bindMeshSurface(const Object3DData &data,
    std::size_t sourceMesh,std::size_t targetMesh,const SurfaceBindingOptions &options);
// Frozen pre-profile-transfer declarations and method symbol.
struct SkinTransferOptions final
{
    double maximumDistance=std::numeric_limits<double>::infinity();
    std::size_t maximumInfluences=0;
    bool skipUnmatched=false,includeUnusedPoints=true;
    std::vector<BoneIndex> sourceToTarget;
};
struct SkinTransferResult final {std::size_t transferred=0,skipped=0;double maximumDistance=0;};
class IIACTOROBJECT_EXPORT SkinBinder final
{
  public:
    [[nodiscard]] static SkinTransferResult transfer(const Object3DData &,std::size_t,Object3DData &,
        std::size_t,const SkinTransferOptions &);
};
}
int main()
{
    try
    {
        auto data=iiActorObject::parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n");
        auto target=data.meshes[0];target.name="follower";
        for(auto &point:target.topology->points)point.position.z=.1;
        data.meshes.push_back(std::move(target));const iiActorObject::SurfaceBindingOptions old{.2,false};
        const auto result=iiActorObject::bindMeshSurface(data,0,1,old);
        if(result.points.size()!=4||result.sourceVertexCount!=4||result.targetPointCount!=4)return 1;
        data.bones={{"root",iiActorObject::NoBone,{}}};
        const iiActorObject::SkinTransferOptions skin{.maximumDistance=.2,.includeUnusedPoints=false};
        const auto transferred=iiActorObject::SkinBinder::transfer(data,0,data,1,skin);
        if(transferred.transferred!=4||transferred.skipped!=0)return 2;
        std::cout<<"Legacy two-field options and original surface-binding symbol passed\n";
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}return 0;
}
