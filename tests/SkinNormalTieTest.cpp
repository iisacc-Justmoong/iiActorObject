#include <iiActorObject/Object3DEngine.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DTemplate.h>
#include <iiActorObject/SkinBinding.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#endif
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
template<class F>void rejects(F f){try{f();}catch(const std::exception&){return;}throw std::runtime_error("Expected rejection");}
struct Fixture final
{
    Object3DData source,target;
    Fixture(bool opposingTarget=false,bool renderTarget=false)
    {
        source=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\n"
            "v 0 0 .000001\nv 1 0 .000001\nv 0 1 .000001\nf 1 2 3\nf 4 6 5\n");
        source.kind=Object3DKind::SkeletalMesh;source.bones={{"outer",NoBone,{}},{"inner",0,{}}};
        for(std::size_t i=0;i<6;++i)source.meshes[0].topology->points[i].weights={{static_cast<BoneIndex>(i<3?0:1),1}};
        source.meshes[0].topology->skinning={SkinningMethod::Blended,{.1,.1,.1,.9,.9,.9}};
        const std::string triangles=opposingTarget?"f 1 2 3\nf 3 2 1\n":"f 1 2 4\nf 2 3 4\nf 3 1 4\n";
        target=parseObject3DObj("v 0 0 .0000006\nv 1 0 .0000006\nv 0 1 .0000006\nv .3 .3 .0000006\n"+triangles);
        target.kind=Object3DKind::SkeletalMesh;target.bones=source.bones;
        for(auto &point:target.meshes[0].topology->points)point.weights={{1,1}};
        if(renderTarget)
        {
            Object3DEngine engine;target=engine.createObject(std::move(target)).get().data();
            target.meshes[0].topology.reset();
        }
    }
};
SkinTransferOptions transferOptions()
{SkinTransferOptions o;o.maximumDistance=.001;o.includeUnusedPoints=false;return o;}
SkinNormalTransferOptions normalOptions()
{SkinNormalTransferOptions o;o.minimumNormalCosine=.5;o.maximumNormalTieDistance=1e-5;return o;}
void opposite(bool renderTarget)
{
    Fixture f(false,renderTarget);auto plain=f.target;
    const auto legacy=SkinBinder::transfer(f.source,0,plain,0,transferOptions(),SkinningProfileTransfer::Source);
    CHECK(legacy.transferred==4);
    const auto first=renderTarget?plain.meshes[0].geometry.vertices[0].weights:plain.meshes[0].topology->points[0].weights;
    CHECK(first.size()==1&&first[0].bone==1);
    SurfaceQueryBudget budget;const auto result=SkinBinder::transferWithNormalTies(f.source,0,f.target,0,transferOptions(),
        SkinningProfileTransfer::Source,normalOptions(),budget);
    CHECK(result.transfer.transferred==4&&result.normalTieRecoveries==4&&result.ambiguousTargetPoints==0);
    CHECK(budget.nodeVisits>0&&budget.triangleTests>0);
    const auto &mesh=f.target.meshes[0];
    for(std::size_t i=0;i<4;++i)
    {
        const auto &weights=renderTarget?mesh.geometry.vertices[i].weights:mesh.topology->points[i].weights;
        CHECK(weights.size()==1&&weights[0].bone==0);
    }
    const auto &profile=renderTarget?mesh.geometry.skinning:mesh.topology->skinning;
    CHECK(profile.method==SkinningMethod::Blended&&profile.blendWeights.size()==4);
    for(double value:profile.blendWeights)CHECK(std::abs(value-.1)<1e-9);
}
void ambiguous()
{
    Fixture f(true);SurfaceQueryBudget budget;
    const auto result=SkinBinder::transferWithNormalTies(f.source,0,f.target,0,transferOptions(),
        SkinningProfileTransfer::Source,normalOptions(),budget);
    CHECK(result.transfer.transferred==3&&result.normalTieRecoveries==0&&result.ambiguousTargetPoints==3);
    for(std::size_t i=0;i<3;++i)CHECK(f.target.meshes[0].topology->points[i].weights[0].bone==1);
}
void limits()
{
    Fixture f;const auto original=f.target;auto options=normalOptions();options.maximumNormalTieDistance=1e-8;
    SurfaceQueryBudget budget;auto result=SkinBinder::transferWithNormalTies(f.source,0,f.target,0,transferOptions(),
        SkinningProfileTransfer::Source,options,budget);
    CHECK(result.normalTieRecoveries==0&&f.target.meshes[0].topology->points[0].weights[0].bone==1);
    f.target=original;options=normalOptions();options.maximumNormalTieDistance=0;budget={};
    result=SkinBinder::transferWithNormalTies(f.source,0,f.target,0,transferOptions(),SkinningProfileTransfer::Source,options,budget);
    CHECK(result.normalTieRecoveries==0&&f.target.meshes[0].topology->points[0].weights[0].bone==1);
    // One point can be changed in the working copy before the next query runs
    // out of budget; the caller's asset must still be unchanged.
    f.target=original;options=normalOptions();budget={};budget.maximumTriangleTests=5;
    rejects([&]{(void)SkinBinder::transferWithNormalTies(f.source,0,f.target,0,transferOptions(),SkinningProfileTransfer::Source,options,budget);});
    CHECK(f.target.meshes[0].topology->points[0].weights[0].bone==1&&f.target.meshes[0].topology->skinning==original.meshes[0].topology->skinning);
    budget={};options.maximumVertices=3;
    rejects([&]{(void)SkinBinder::transferWithNormalTies(f.source,0,f.target,0,transferOptions(),SkinningProfileTransfer::Source,options,budget);});
    options=normalOptions();options.minimumNormalCosine=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{(void)SkinBinder::transferWithNormalTies(f.source,0,f.target,0,transferOptions(),SkinningProfileTransfer::Source,options,budget);});
    CHECK(f.target.meshes[0].topology->points[0].weights[0].bone==1);
}
void editor()
{
    Fixture f;Object3DEngine engine;const auto source=engine.createObject(f.source),target=engine.createObject(f.target);
    const auto session=engine.editObject(target);auto &e=session.get();const auto initial=e.object();const auto revision=e.revision();
    SurfaceQueryBudget budget;const auto result=e.transferWeightsWithNormalTies(0,source.get(),0,transferOptions(),
        SkinningProfileTransfer::Source,normalOptions(),budget);
    CHECK(result.transfer.transferred==4&&result.normalTieRecoveries==4&&e.revision()==revision+1);
    CHECK(e.object().get().data().meshes[0].topology->points[0].weights[0].bone==0);
    CHECK(e.object().get().data().meshes[0].topology->skinning.method==SkinningMethod::Blended);
    CHECK(initial.get().data().meshes[0].topology->points[0].weights[0].bone==1);
    CHECK(source.get().data().meshes[0].topology->points[3].weights[0].bone==1);
    const auto unchanged=e.object();const auto unchangedRevision=e.revision();const auto unchangedCount=engine.objectCount();budget={};
    const auto repeated=e.transferWeightsWithNormalTies(0,source.get(),0,transferOptions(),
        SkinningProfileTransfer::Source,normalOptions(),budget);
    CHECK(repeated.transfer.transferred==4&&e.object()==unchanged&&e.revision()==unchangedRevision&&engine.objectCount()==unchangedCount);
    CHECK(e.undo()&&e.object().get().data().meshes[0].topology->points[0].weights[0].bone==1);
    CHECK(e.object().get().data().meshes[0].topology->skinning.method==SkinningMethod::Linear);
    CHECK(e.redo()&&e.object().get().data().meshes[0].topology->points[0].weights[0].bone==0);
    Object3DEngine restored;auto project=importObject3DProject(restored.reference(),exportObject3DProject(session));
    auto &r=project.editor.get();CHECK(r.object().get().data().meshes[0].topology->points[0].weights[0].bone==0);
    CHECK(r.undo()&&r.object().get().data().meshes[0].topology->points[0].weights[0].bone==1);
    CHECK(r.redo()&&r.object().get().data().meshes[0].topology->skinning.method==SkinningMethod::Blended);
}
void layeredEditor()
{
    Fixture f;Object3DEngine engine;const auto source=engine.createObject(f.source),target=engine.createObject(f.target);
    Object3DTemplateDefinition definition;definition.base=target;TemplateTarget shift{"shift",0,{},0,1};
    for(std::uint32_t i=0;i<4;++i)shift.deltas.push_back({i,{0,0,.0000001}});
    definition.targets.push_back(std::move(shift));const auto session=engine.editTemplate(engine.createTemplate(std::move(definition)));
    auto &e=session.get();e.setTemplateValues(std::array{TemplateValue{"shift",.5}});SurfaceQueryBudget budget;
    const auto result=e.transferWeightsWithNormalTies(0,source.get(),0,transferOptions(),SkinningProfileTransfer::Source,normalOptions(),budget);
    CHECK(result.normalTieRecoveries==4&&e.layers().size()==1);const auto layer=e.layers()[0].id;
    CHECK(e.object().get().data().meshes[0].topology->points[0].weights[0].bone==0);
    e.setLayerEnabled(layer,false);CHECK(e.object().get().data().meshes[0].topology->points[0].weights[0].bone==1);
    CHECK(e.object().get().data().meshes[0].topology->skinning.method==SkinningMethod::Linear);
    e.setLayerEnabled(layer,true);e.setTemplateValues(std::array{TemplateValue{"shift",1}});
    CHECK(e.object().get().data().meshes[0].topology->points[0].weights[0].bone==0);
    e.setLayerWeight(layer,.5);const auto &weights=e.object().get().data().meshes[0].topology->points[0].weights;
    CHECK(weights.size()==2&&std::abs(weights[0].weight-.5)<1e-9&&std::abs(weights[1].weight-.5)<1e-9);
    Object3DEngine restored;auto p=importObject3DProject(restored.reference(),exportObject3DProject(session));auto &r=p.editor.get();
    CHECK(r.layers().size()==1&&r.layers()[0].weight==.5&&r.object().get().data().meshes[0].topology->points[0].weights.size()==2);
    r.setLayerEnabled(layer,false);CHECK(r.object().get().data().meshes[0].topology->points[0].weights[0].bone==1);
    CHECK(r.undo()&&r.redo());
}
void editorAtomic()
{
    Fixture f;Object3DEngine engine;const auto source=engine.createObject(f.source),target=engine.createObject(f.target);
    const auto session=engine.editObject(target);auto &e=session.get();SurfaceQueryBudget budget;
    (void)e.transferWeightsWithNormalTies(0,source.get(),0,transferOptions(),SkinningProfileTransfer::Source,normalOptions(),budget);
    CHECK(e.undo());const auto before=e.object();const auto revision=e.revision();const auto count=engine.objectCount();
    budget={};budget.maximumTriangleTests=5;
    rejects([&]{(void)e.transferWeightsWithNormalTies(0,source.get(),0,transferOptions(),SkinningProfileTransfer::Source,normalOptions(),budget);});
    CHECK(e.object()==before&&e.revision()==revision&&engine.objectCount()==count&&budget.triangleTests==5);
    CHECK(e.redo()&&e.object().get().data().meshes[0].topology->points[0].weights[0].bone==0);
    auto miss=transferOptions();miss.maximumDistance=0;miss.skipUnmatched=true;budget={};
    const auto stable=e.object();const auto version=e.revision();const auto assets=engine.objectCount();
    const auto skipped=e.transferWeightsWithNormalTies(0,source.get(),0,miss,SkinningProfileTransfer::Source,normalOptions(),budget);
    CHECK(skipped.transfer.transferred==0&&skipped.transfer.skipped==4&&e.object()==stable&&e.revision()==version&&engine.objectCount()==assets);
}
#if IIACTOROBJECT_HAS_NSCHARACTER
void characterEditor()
{
    Fixture f;Object3DEngine engine;const auto source=engine.createObject(f.source),target=engine.createObject(f.target);
    const NSCharacter::CharacterAsset sourceView(source);NSCharacter::CharacterEditor character(engine.editObject(target));
    const auto initial=character.asset();SurfaceQueryBudget budget;
    const auto result=character.transferWeightsWithNormalTies(0,sourceView,0,transferOptions(),SkinningProfileTransfer::Source,normalOptions(),budget);
    CHECK(result.normalTieRecoveries==4&&character.asset()->data().meshes[0].topology->points[0].weights[0].bone==0);
    CHECK(initial->data().meshes[0].topology->points[0].weights[0].bone==1);
    CHECK(character.undo()&&character.asset()->data().meshes[0].topology->points[0].weights[0].bone==1);
    CHECK(character.redo()&&character.asset()->data().meshes[0].topology->points[0].weights[0].bone==0);
}
void characterLifetime()
{
    std::unique_ptr<NSCharacter::CharacterEditor> character;std::optional<NSCharacter::CharacterAsset> sourceView;
    {
        Fixture f;Object3DEngine engine;const auto source=engine.createObject(f.source),target=engine.createObject(f.target);
        sourceView.emplace(source);character=std::make_unique<NSCharacter::CharacterEditor>(engine.editObject(target));
        CHECK(sourceView->valid()&&character->session().valid());
    }
    CHECK(!sourceView->valid()&&!character->session().valid());SurfaceQueryBudget budget;
    rejects([&]{(void)character->transferWeightsWithNormalTies(0,*sourceView,0,transferOptions(),SkinningProfileTransfer::Source,normalOptions(),budget);});
}
#endif
int main(int argc,char **argv)
{
    try
    {
        if(argc!=2)return 2;const std::string_view mode=argv[1];
        if(mode=="opposite")opposite(false);else if(mode=="render")opposite(true);else if(mode=="ambiguous")ambiguous();
        else if(mode=="limits")limits();else if(mode=="editor")editor();else if(mode=="layered")layeredEditor();else if(mode=="editor_atomic")editorAtomic();
#if IIACTOROBJECT_HAS_NSCHARACTER
        else if(mode=="character")characterEditor();else if(mode=="character_lifetime")characterLifetime();
#endif
        else return 2;
        std::cout<<mode<<" passed\n";
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
