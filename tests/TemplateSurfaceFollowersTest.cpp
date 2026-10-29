#include "../src/Geometry/Object3DTemplateSurfaceFollowers.h"
#include <array>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool value,const std::source_location &where=std::source_location::current())
{if(!value)throw std::runtime_error("Template follower test failed at line "+std::to_string(where.line()));}
template<class F>void rejects(F action,std::string_view message={})
{
    try{action();}
    catch(const std::exception &error)
    {if(!message.empty()&&error.what()!=message)throw std::runtime_error("Expected: "+std::string(message)+"; received: "+error.what());return;}
    throw std::runtime_error("Invalid surface follower accepted");
}
Object3DMesh renderMesh(double x=0,BoneIndex bone=0)
{
    Object3DMesh mesh;mesh.geometry.vertices.push_back({{x,0,0},{0,0,1},{{bone,1}}});return mesh;
}
Object3DMesh authoredMesh(double x=0,BoneIndex bone=0)
{
    Object3DMesh mesh;mesh.topology.emplace();mesh.topology->points.push_back({{x,0,0},{{bone,1}}});return mesh;
}
class StubOperations final : public TemplateSurfaceFollowerOperations
{
  public:
    mutable std::vector<std::pair<char,std::size_t>> calls;
    mutable std::vector<bool> hasBasis;
    bool failTransfer=false;
    void fit(Object3DData &data,const SurfaceBinding &binding,const std::optional<SurfaceBindingBasis> &basis) const override
    {
        calls.push_back({'F',binding.targetMesh});hasBasis.push_back(basis.has_value());
        auto &mesh=data.meshes.at(binding.targetMesh);
        if(mesh.topology)mesh.topology->points.at(0).position.y=3;
        else mesh.geometry.vertices.at(0).position.y=3;
    }
    void transfer(Object3DData &data,const SurfaceBinding &binding,const std::optional<SurfaceBindingBasis> &basis) const override
    {
        calls.push_back({'T',binding.targetMesh});hasBasis.push_back(basis.has_value());
        if(failTransfer)throw std::runtime_error("Injected transfer failure");
        const auto &source=data.meshes.at(binding.sourceMesh);auto &target=data.meshes.at(binding.targetMesh);
        if(source.topology&&target.topology)
        {target.topology->points.at(0).weights=source.topology->points.at(0).weights;target.topology->skinning=source.topology->skinning;}
        else
        {target.geometry.vertices.at(0).weights=source.geometry.vertices.at(0).weights;target.geometry.skinning=source.geometry.skinning;}
    }
};
}
int main()
{
    Object3DData before,after,current;
    before.meshes={renderMesh(0,0),renderMesh(2,0)};current=before;
    current.meshes[0].geometry.vertices[0].weights={{1,1}};
    current.meshes[0].geometry.skinning.method=SkinningMethod::DualQuaternion;
    after.meshes={before.meshes[0],renderMesh(),before.meshes[1],renderMesh(),renderMesh()};
    auto staged=after;
    SurfaceBinding low;low.sourceMesh=3;low.targetMesh=1;
    SurfaceBinding high;high.sourceMesh=0;high.targetMesh=3;
    SurfaceBinding tail;tail.sourceMesh=0;tail.targetMesh=4;
    const std::vector<SurfaceBinding> bindings{low,high,tail};
    const SurfaceBindingBasis basis{{{1,0,0},{0,1,0},{0,0,1}}};
    const std::vector<std::optional<SurfaceBindingBasis>> bases{std::nullopt,basis,std::nullopt};
    const std::vector<std::optional<std::size_t>> newToOld{0,std::nullopt,1,std::nullopt,std::nullopt};
    const std::set<std::size_t> added{1,3,4};StubOperations ops;
    const auto steps=addTemplateSurfaceFollowers(staged,before,after,current,newToOld,added,bindings,bases,ops);
    check(ops.calls==std::vector<std::pair<char,std::size_t>>({{'F',3},{'T',3},{'F',4},{'T',4},{'F',1},{'T',1}}));
    check(ops.hasBasis==std::vector<bool>({true,true,false,false,false,false}));
    check(steps.size()==3&&steps[0].binding.targetMesh==3&&steps[0].basis==basis&&steps[2].binding.targetMesh==1);
    for(const auto index:added)
    {
        check(staged.meshes[index].geometry.vertices[0].position.y==3);
        check(staged.meshes[index].geometry.vertices[0].weights[0].bone==1);
        check(staged.meshes[index].geometry.skinning.method==SkinningMethod::DualQuaternion);
    }
    check(staged.meshes[0].geometry.vertices[0].weights[0].bone==0&&after.meshes[3].geometry.vertices[0].position.y==0);

    Object3DData authoredBefore,authoredAfter,authoredCurrent;
    authoredBefore.meshes={authoredMesh(0,0)};authoredAfter.meshes={authoredBefore.meshes[0],authoredMesh()};
    authoredCurrent=authoredBefore;authoredCurrent.meshes[0].topology->points[0].weights={{2,1}};
    authoredCurrent.meshes[0].topology->skinning.method=SkinningMethod::DualQuaternion;
    auto authored=authoredAfter;SurfaceBinding authoredBinding;authoredBinding.sourceMesh=0;authoredBinding.targetMesh=1;
    StubOperations authoredOps;
    applyTemplateSurfaceFollower(authored,authoredBinding,std::nullopt,&authoredCurrent.meshes[0],
        TemplateSurfaceFollowerKind::Replacement,authoredOps);
    check(authored.meshes[1].topology->points[0].weights[0].bone==2&&
        authored.meshes[1].topology->skinning.method==SkinningMethod::DualQuaternion);
    check(authored.meshes[0].topology->points[0].weights[0].bone==0);
    authored=authoredAfter;authoredOps.calls.clear();
    const auto authoredSteps=addTemplateSurfaceFollowers(authored,authoredBefore,authoredAfter,authoredCurrent,
        std::array<std::optional<std::size_t>,2>{0,std::nullopt},std::set<std::size_t>{1},
        std::span(&authoredBinding,1),{},authoredOps);
    check(authoredSteps.size()==1&&authored.meshes[1].topology->points[0].weights[0].bone==2&&
        authored.meshes[1].topology->skinning.method==SkinningMethod::DualQuaternion);

    auto shifted=after;shifted.meshes[0].geometry.vertices[0].position.x=.1;auto trial=after;
    StubOperations errors;
    rejects([&]{(void)addTemplateSurfaceFollowers(trial,before,shifted,current,newToOld,added,bindings,bases,errors);},
        "Added surface neutral source changed");
    check(errors.calls.empty()&&after.meshes[3].geometry.vertices[0].position.y==0);
    auto invalid=bindings;invalid[1].sourceMesh=9;trial=after;errors.calls.clear();
    rejects([&]{(void)addTemplateSurfaceFollowers(trial,before,after,current,newToOld,added,invalid,bases,errors);},
        "Added surface source is invalid");
    auto cycle=bindings;cycle[1].sourceMesh=1;trial=after;errors.calls.clear();
    rejects([&]{(void)addTemplateSurfaceFollowers(trial,before,after,current,newToOld,added,cycle,bases,errors);},
        "Added surface binding graph is cyclic");
    check(trial.meshes[4].geometry.vertices[0].position.y==3&&after.meshes[4].geometry.vertices[0].position.y==0);
    auto missing=bindings;missing.pop_back();trial=after;errors.calls.clear();
    rejects([&]{(void)addTemplateSurfaceFollowers(trial,before,after,current,newToOld,added,missing,{},errors);},
        "Template surface replacement requires a follower binding");
    auto brokenCurrent=authoredCurrent;brokenCurrent.meshes[0].topology->points.clear();authored=authoredAfter;errors.calls.clear();
    rejects([&]{applyTemplateSurfaceFollower(authored,authoredBinding,std::nullopt,&brokenCurrent.meshes[0],
        TemplateSurfaceFollowerKind::Replacement,errors);},"Replacement source point identities changed");
    check(errors.calls==std::vector<std::pair<char,std::size_t>>({{'F',1}}));
    auto noTopology=authoredAfter;noTopology.meshes[0]=renderMesh();authored=authoredAfter;errors.calls.clear();
    rejects([&]{(void)addTemplateSurfaceFollowers(authored,authoredBefore,noTopology,authoredCurrent,
        std::array<std::optional<std::size_t>,2>{0,std::nullopt},std::set<std::size_t>{1},
        std::span(&authoredBinding,1),{},errors);},"Added surface source representation changed");
    check(errors.calls.empty());
    rejects([&]{applyTemplateSurfaceFollower(authored,authoredBinding,std::nullopt,nullptr,
        TemplateSurfaceFollowerKind::Replacement,errors);},"Replacement source skin is missing");
    rejects([&]{(void)addTemplateSurfaceFollowers(trial,before,after,current,
        std::array<std::optional<std::size_t>,1>{0},added,bindings,bases,errors);},"Added surface slot mapping is invalid");
    errors.failTransfer=true;authored=authoredAfter;
    rejects([&]{applyTemplateSurfaceFollower(authored,authoredBinding,std::nullopt,&authoredCurrent.meshes[0],
        TemplateSurfaceFollowerKind::Replacement,errors);},"Injected transfer failure");
    check(authored.meshes[1].topology->points[0].position.y==3&&authoredAfter.meshes[1].topology->points[0].position.y==0);
}
