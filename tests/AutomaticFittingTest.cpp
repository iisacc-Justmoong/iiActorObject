#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DTemplate.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
template<class F>std::string rejects(F f){try{f();}catch(const std::exception &e){return e.what();}throw std::runtime_error("Expected rejection");}
void near(double a,double b){if(std::abs(a-b)>=1e-9)throw std::runtime_error("Expected "+std::to_string(b)+", got "+std::to_string(a));}
double z(const Object3DHandle &object,std::size_t mesh=1){return object.get().data().meshes[mesh].topology->points[0].position.z;}
struct Scene final
{
    Object3DEngine engine;
    Object3DTemplateDefinition definition;
    Scene(bool followers=false,bool badge=false)
    {
        auto data=parseObject3DObj("v -1 -1 -1\nv 1 -1 -1\nv 1 1 -1\nv -1 1 -1\nv -1 -1 1\nv 1 -1 1\nv 1 1 1\nv -1 1 1\nf 1 4 3 2\nf 5 6 7 8\nf 1 2 6 5\nf 2 3 7 6\nf 3 4 8 7\nf 4 1 5 8\n");
        data.meshes[0].name="Body";
        auto cloth=parseObject3DObj("v -.1 -.1 .999\nv .1 -.1 .999\nv 0 .1 .999\nf 1 2 3\n").meshes[0];cloth.name="Cloth";
        TopologyMorph detail;detail.name="Detail";detail.sourceTarget=9;detail.deltas={{0,{.0001,0,0}}};cloth.topology->morphTargets.push_back(detail);data.meshes.push_back(cloth);
        if(badge){cloth.name="Badge";for(auto &p:cloth.topology->points)p.position.z+=.005;data.meshes.push_back(cloth);}
        definition.base=engine.createObject(std::move(data));
        TemplateTarget shift;shift.name="body-up";for(std::uint32_t i=0;i<8;++i)shift.deltas.push_back({i,{0,0,.02}});definition.targets.push_back(shift);
        if(followers)definition.surfaces.push_back(bindMeshSurface(definition.base.get().data(),0,1));
        if(badge)definition.surfaces.push_back(bindMeshSurface(definition.base.get().data(),1,2));
    }
    SurfaceFittingRule rule() const
    {
        SurfaceFittingRule result;result.targetMesh=1;result.obstacleMeshes={0};SurfaceFitOptions o;o.edgeStiffness=0;result.fitting=SurfaceFitSettings(o);return result;
    }
    Object3DTemplateHandle program(){return engine.createTemplate(definition);}
};
void settings()
{
    std::vector<double> mobility{1,.5,0};SurfaceFitOptions o;o.mobility=mobility;o.clearance=.003;SurfaceFitSettings owned(o);mobility[1]=0;
    CHECK(owned.options().mobility[1]==.5&&owned.options().mobility.data()!=mobility.data());SurfaceFitSettings copied=owned;CHECK(copied==owned);
    auto moved=std::move(copied);CHECK(moved==owned&&moved.options().mobility[1]==.5&&moved.options().clearance==.003);
    SurfaceFitOptions noEscape;noEscape.escapeConcavities=false;CHECK(SurfaceFitSettings(noEscape)!=SurfaceFitSettings{});
}
void generation()
{
    Scene s;s.definition.fittings={s.rule()};auto p=s.program();auto a=p.get().generate(),b=p.get().generate();near(z(a),1.002);near(z(b),1.002);
    auto shifted=p.get().generate(std::array{TemplateValue{"body-up",1}});near(z(shifted),1.022);near(z(s.definition.base),.999);
    const auto &mesh=a.get().data().meshes[1];CHECK(mesh.topology->layout==s.definition.base.get().data().meshes[1].topology->layout);CHECK(mesh.topology->morphTargets[0].sourceTarget==9);
    auto data=shifted.get().data();auto result=applyObject3DSurfaceFitting(data,s.definition.surfaces,s.definition.fittings);CHECK(result.size()==1&&result[0].result.changedPoints==0);
}
void followers()
{
    Scene s(true,true);s.definition.fittings={s.rule()};auto p=s.program();auto a=p.get().generate();near(z(a),1.002);near(z(a,2),1.007);
    auto b=p.get().generate(std::array{TemplateValue{"body-up",1}});near(z(b),1.022);near(z(b,2),1.027);
    auto e=s.engine.editTemplate(p);near(z(e.get().object(),2),1.007);
}
void basisFollowers()
{
    Scene s(true,true);auto data=s.definition.base.get().data();
    std::vector<SurfaceControlPoint> points;
    for(std::uint32_t i=0;i<3;++i)points.push_back({i,{i,i,i},{1,0,0},{.02,0,.005}});
    std::array<std::optional<SurfaceBindingAxisScale>,3> scales{};
    scales[0]=SurfaceBindingAxisScale{0,1,.1};
    auto binding=bindMeshControlPoints(data,1,2,points,scales);
    const SurfaceBindingBasis basis{Vector3{2,0,0},Vector3{0,1,0},Vector3{0,0,1}};
    const auto positions=evaluateMeshSurfaceBindingWithBasis(data,binding,basis);
    for(std::size_t i=0;i<positions.size();++i)data.meshes[2].topology->points[i].position=positions[i];
    s.definition.base=s.engine.createObject(std::move(data));
    s.definition.surfaces[1]=std::move(binding);s.definition.surfaceOffsetBases={std::nullopt,basis};
    s.definition.fittings={s.rule()};auto editor=s.engine.editTemplate(s.program());auto &e=editor.get();
    near(z(e.object()),1.002);near(z(e.object(),2),1.007);
    near(e.object().get().data().meshes[2].topology->points[0].position.x,-.06);
    e.setTemplateValues(std::array{TemplateValue{"body-up",1}});
    near(z(e.object()),1.022);near(z(e.object(),2),1.027);
    // A host snapshot changes the cloth and its already-following accessory.
    // Import must remove the follower contribution before creating the edit layer.
    const auto source=e.object();auto externalData=source.get().data();
    externalData.meshes[1].topology->points[1].position.x+=.1;
    for(auto &p:externalData.meshes[2].topology->points)p.position.x+=.02;
    externalData.meshes[2].topology->points[1].position.x+=.1;
    const auto edited=s.engine.createObject(std::move(externalData));
    const auto ids=makeObject3DExternalIdentity(source.get().data(),"basis-followers");
    Object3DExternalOptions external;external.document=ids.document;
    const auto report=e.applyExternalEdits(source.get(),{source.get(),ids},{edited.get(),ids},external);
    CHECK(report.positions==4&&report.layer);near(e.object().get().data().meshes[2].topology->points[0].position.x,-.04);
    e.setTemplateValues(std::array{TemplateValue{"body-up",.5}});near(z(e.object(),2),1.017);
    near(e.object().get().data().meshes[2].topology->points[0].position.x,-.04);
    e.setTemplateValues(std::array{TemplateValue{"body-up",1}});
    e.moveControlPoints(1,std::array{TopologyPointMove{1,{.1,0,0}}});
    near(e.object().get().data().meshes[2].topology->points[0].position.x,-.02);
    auto accessory=e.object().get().data().meshes[2];accessory.name="Extra";
    for(auto &p:accessory.topology->points)p.position.z+=.05;
    SurfaceAttachmentOptions options;options.transferSkin=false;
    CHECK(e.attachSurface(std::move(accessory),1,options).mesh==3);
    const auto &bases=e.templateProgram().get().definition().surfaceOffsetBases;
    CHECK(bases.size()==3&&!bases[0]&&bases[1]&&!bases[2]);
    const auto bytes=exportObject3DProject(editor);CHECK(bytes[8]==25);
    Object3DEngine restored;auto project=importObject3DProject(restored.reference(),bytes);
    const auto &restoredBases=project.editor.get().templateProgram().get().definition().surfaceOffsetBases;
    CHECK(restoredBases.size()==3&&!restoredBases[0]&&restoredBases[1]&&!restoredBases[2]);
    near(project.editor.get().object().get().data().meshes[2].topology->points[0].position.x,-.02);
    CHECK(project.editor.get().undo()&&project.editor.get().redo());
}
void graph()
{
    Scene s(true);auto rule=s.rule();rule.targetMesh=0;rule.obstacleMeshes={1};
    CHECK(rejects([&]{validateObject3DSurfaceFitting(s.definition.base.get().data(),s.definition.surfaces,std::span(&rule,1));}).find("yclic")!=std::string::npos);
    rule=s.rule();std::array duplicates{rule,rule};rejects([&]{validateObject3DSurfaceFitting(s.definition.base.get().data(),{},duplicates);});
    rule.obstacleMeshes={1};rejects([&]{validateObject3DSurfaceFitting(s.definition.base.get().data(),{},std::span(&rule,1));});
    rule=s.rule();SurfaceFitOptions preview;preview.allowUnresolved=true;rule.fitting=SurfaceFitSettings(preview);rejects([&]{validateObject3DSurfaceFitting(s.definition.base.get().data(),{},std::span(&rule,1));});
}
void edits()
{
    Scene s(true,true);s.definition.fittings={s.rule()};auto e=s.engine.editTemplate(s.program());std::vector<TopologyPointMove> moves;for(std::uint32_t i=0;i<8;++i)moves.push_back({i,{0,0,.01}});
    e.get().moveControlPoints(0,moves);near(z(e.get().object()),1.012);near(z(e.get().object(),2),1.017);CHECK(e.get().layers().size()==1);
    e.get().setTemplateValues(std::array{TemplateValue{"body-up",.5}});near(z(e.get().object()),1.022);near(z(e.get().object(),2),1.027);
    e.get().moveControlPoints(1,std::array{TopologyPointMove{0,{.001,0,0}}});near(e.get().object().get().data().meshes[1].topology->points[0].position.x,-.099);
    auto rule=s.rule();rule.enabled=false;e.get().setSurfaceFittings(std::span(&rule,1));near(z(e.get().object()),1.019);CHECK(e.get().layers().size()==2);
}
void history()
{
    Scene s;auto e=s.engine.editObject(s.definition.base);auto rule=s.rule();e.get().setSurfaceFittings(std::span(&rule,1));CHECK(e.get().layered()&&e.get().layers().empty());near(z(e.get().object()),1.002);
    const auto revision=e.get().revision();e.get().setSurfaceFittings(std::span(&rule,1));CHECK(e.get().revision()==revision);
    rule.enabled=false;e.get().setSurfaceFittings(std::span(&rule,1));near(z(e.get().object()),.999);CHECK(e.get().undo());near(z(e.get().object()),1.002);CHECK(e.get().redo());near(z(e.get().object()),.999);
    e.get().setSurfaceFittings({});CHECK(e.get().surfaceFittings().empty()&&e.get().undo()&&!e.get().surfaceFittings()[0].enabled);
    e.get().detachTemplate();CHECK(!e.get().layered()&&e.get().surfaceFittings().empty()&&e.get().undo()&&e.get().layered());
}
void masks()
{
    Scene s;auto data=s.definition.base.get().data();data.meshes[1].topology->points[1].position.z=1.002;s.definition.base=s.engine.createObject(std::move(data));
    std::vector<double> mobility{1,0,1};SurfaceFitOptions o;o.mobility=mobility;o.edgeStiffness=0;auto rule=s.rule();rule.fitting=SurfaceFitSettings(o);mobility[0]=0;s.definition.fittings={rule};
    auto e=s.engine.editTemplate(s.program());for(const auto &p:e.get().object().get().data().meshes[1].topology->points)near(p.position.z,1.002);
    const auto revision=e.get().revision();rejects([&]{e.get().setTemplateValues(std::array{TemplateValue{"body-up",1}});});CHECK(e.get().revision()==revision&&e.get().templateValues().empty());
}
void serialization(const std::filesystem::path &path)
{
    Scene s(true);auto rule=s.rule();const std::array mobility{1.,.5,1.};SurfaceFitOptions o;o.mobility=mobility;o.edgeStiffness=0;o.escapeConcavities=false;o.maximumDisplacement=std::numeric_limits<double>::infinity();rule.fitting=SurfaceFitSettings(o);s.definition.fittings={rule};
    auto e=s.engine.editTemplate(s.program());e.get().moveControlPoints(1,std::array{TopologyPointMove{0,{.001,0,.01}}});e.get().renameLayer(e.get().layers()[0].id,"Evaluated surface fixture");auto bytes=exportObject3DProject(e);CHECK(bytes[8]==10);
    if(!path.empty()){std::filesystem::create_directories(path.parent_path());std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(f.good());}
    Object3DEngine restored;auto p=importObject3DProject(restored.reference(),bytes);CHECK(p.editor.get().surfaceFittings().size()==1&&p.editor.get().surfaceFittings()[0]==rule);
    CHECK(std::isinf(p.editor.get().surfaceFittings()[0].fitting.options().maximumDisplacement)&&!p.editor.get().surfaceFittings()[0].fitting.options().escapeConcavities);p.editor.get().setTemplateValues(std::array{TemplateValue{"body-up",.5}});
    near(z(p.editor.get().object()),1.022);CHECK(p.editor.get().layers()[0].evaluation==Object3DLayerEvaluation::EvaluatedSurface);
    near(p.editor.get().object().get().data().meshes[1].topology->points[0].position.x,-.099);CHECK(p.editor.get().undo()&&p.editor.get().redo());
}
void atomic()
{
    Scene s;auto rule=s.rule();SurfaceFitOptions o;o.edgeStiffness=0;o.maximumDisplacement=.01;o.iterations=4;rule.fitting=SurfaceFitSettings(o);s.definition.fittings={rule};auto e=s.engine.editTemplate(s.program());
    const auto object=e.get().object();const auto revision=e.get().revision();const auto assets=s.engine.objectCount();rejects([&]{e.get().setTemplateValues(std::array{TemplateValue{"body-up",1}});});
    CHECK(e.get().object()==object&&e.get().revision()==revision&&e.get().templateValues().empty()&&s.engine.objectCount()==assets);
    rule.obstacleMeshes={999};const auto programs=s.engine.templateCount();rejects([&]{e.get().setSurfaceFittings(std::span(&rule,1));});CHECK(e.get().revision()==revision&&s.engine.templateCount()==programs);
}
void budgets()
{
    Scene s;auto e=s.engine.editObject(s.definition.base);auto rule=s.rule();SurfaceFitOptions o;o.maximumNodeVisits=1;rule.fitting=SurfaceFitSettings(o);const auto revision=e.get().revision();
    const auto assets=s.engine.objectCount(),programs=s.engine.templateCount();rejects([&]{e.get().setSurfaceFittings(std::span(&rule,1));});CHECK(e.get().revision()==revision&&!e.get().layered()&&s.engine.objectCount()==assets&&s.engine.templateCount()==programs);
    auto data=s.definition.base.get().data();data.meshes.push_back(data.meshes[0]);data.meshes.back().name="Other body";rule=s.rule();rule.obstacleMeshes={0,2};rule.collider.maximumPoints=8;
    rejects([&]{validateObject3DSurfaceFitting(data,{},std::span(&rule,1));});
}
void evaluatedSculpt()
{
    Scene s;s.definition.fittings={s.rule()};auto editor=s.engine.editTemplate(s.program());auto &e=editor.get();
    near(z(e.object()),1.002);const std::array move{TopologyPointMove{0,{0,0,.01}}};
    e.moveControlPoints(1,move);near(z(e.object()),1.012);
    CHECK(e.layers()[0].evaluation==Object3DLayerEvaluation::EvaluatedSurface);
    e.moveControlPoints(1,move);near(z(e.object()),1.022);
    e.setTemplateValues(std::array{TemplateValue{"body-up",1}});near(z(e.object()),1.042);
    auto rule=s.rule();rule.enabled=false;e.setSurfaceFittings(std::span(&rule,1));near(z(e.object()),1.019);
}
void evaluatedExternal()
{
    Scene s;s.definition.fittings={s.rule()};auto editor=s.engine.editTemplate(s.program());auto &e=editor.get();const auto source=e.object();
    const auto ids=makeObject3DExternalIdentity(source.get().data(),"evaluated-external");auto data=source.get().data();
    data.meshes[1].topology->points[0].position.z+=.01;const auto edited=s.engine.createObject(std::move(data));Object3DExternalOptions options;options.document=ids.document;
    const auto report=e.applyExternalEdits(source.get(),{source.get(),ids},{edited.get(),ids},options);CHECK(report.positions==1&&report.layer);near(z(e.object()),1.012);
    options.replaceLayer=report.layer;(void)e.applyExternalEdits(source.get(),{source.get(),ids},{edited.get(),ids},options);near(z(e.object()),1.012);CHECK(e.layers().size()==1);
    Object3DEngine restored;auto imported=importObject3DProject(restored.reference(),exportObject3DProject(editor));
    imported.editor.get().setTemplateValues(std::array{TemplateValue{"body-up",1}});near(z(imported.editor.get().object()),1.032);CHECK(imported.editor.get().undo()&&imported.editor.get().redo());
}
void evaluatedOrder()
{
    Scene s;s.definition.fittings={s.rule()};auto editor=s.engine.editTemplate(s.program());auto &e=editor.get();
    e.moveControlPoints(1,std::array{TopologyPointMove{0,{0,0,.01}}});const auto outward=e.layers()[0].id;
    e.moveControlPoints(1,std::array{TopologyPointMove{0,{0,0,-.015}}});near(z(e.object()),1.002);const auto inward=e.layers()[1].id;
    e.moveLayer(inward,0);near(z(e.object()),1.012);CHECK(e.undo());near(z(e.object()),1.002);CHECK(e.redo());near(z(e.object()),1.012);
    e.setLayerWeight(outward,.5);near(z(e.object()),1.007);e.setLayerPointMasks(outward,std::array{MeshPointMask{1,{.5,1,1}}});near(z(e.object()),1.0045);
    e.setLayerWeight(outward,0);near(z(e.object()),1.002);e.setLayerEnabled(inward,false);near(z(e.object()),1.002);
}
void evaluatedFollowers(bool external)
{
    Scene s(true,true);s.definition.fittings={s.rule()};auto editor=s.engine.editTemplate(s.program());auto &e=editor.get();
    if(external)
    {
        const auto source=e.object();auto data=source.get().data();for(auto &p:data.meshes[1].topology->points)p.position.z+=.01;
        const auto edited=s.engine.createObject(std::move(data));const auto ids=makeObject3DExternalIdentity(source.get().data(),"evaluated-followers");Object3DExternalOptions options;options.document=ids.document;
        const auto r=e.applyExternalEdits(source.get(),{source.get(),ids},{edited.get(),ids},options);CHECK(r.positions==3&&r.layer);near(z(e.object()),1.012);near(z(e.object(),2),1.007);
        e.setTemplateValues(std::array{TemplateValue{"body-up",.5}});options.replaceLayer=r.layer;
        (void)e.applyExternalEdits(source.get(),{source.get(),ids},{edited.get(),ids},options);near(z(e.object()),1.022);near(z(e.object(),2),1.017);CHECK(e.layers().size()==1);
    }
    else
    {
        e.moveControlPoints(1,std::array{TopologyPointMove{0,{0,0,.01}},TopologyPointMove{1,{0,0,.01}},TopologyPointMove{2,{0,0,.01}}});
        near(z(e.object()),1.012);near(z(e.object(),2),1.017);
        e.setTemplateValues(std::array{TemplateValue{"body-up",1}});near(z(e.object()),1.032);near(z(e.object(),2),1.037);
    }
}
void evaluatedAtomic()
{
    Scene s;auto rule=s.rule();SurfaceFitOptions o;o.edgeStiffness=0;o.maximumDisplacement=.01;rule.fitting=SurfaceFitSettings(o);s.definition.fittings={rule};
    auto editor=s.engine.editTemplate(s.program());auto &e=editor.get();e.moveControlPoints(1,std::array{TopologyPointMove{0,{0,0,.01}}});
    const auto object=e.object();const auto revision=e.revision();const auto assets=s.engine.objectCount();const auto layers=e.layers().size();
    rejects([&]{e.moveControlPoints(1,std::array{TopologyPointMove{0,{0,0,-.1}}});});CHECK(e.object()==object&&e.revision()==revision&&e.layers().size()==layers&&s.engine.objectCount()==assets);
}
void legacyV9(const std::filesystem::path &path)
{
    std::ifstream f(path,std::ios::binary);CHECK(f.good());const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});CHECK(bytes[8]==9);
    Object3DEngine engine;auto p=importObject3DProject(engine.reference(),bytes);auto &e=p.editor.get();CHECK(e.surfaceFittings().size()==1);
    for(const auto &layer:e.layers())CHECK(layer.evaluation==Object3DLayerEvaluation::Deferred);
    near(z(e.object()),e.layers().size()==2?1.009:1.002);CHECK(exportObject3DProject(p.editor)[8]==9);
    e.setTemplateValues(std::array{TemplateValue{"body-up",.5}});near(z(e.object()),e.layers().size()==2?1.019:1.012);
}
int main(int argc,char **argv)
{
    try
    {
        if(argc<2)return 2;const std::string c=argv[1];if(c=="settings")settings();else if(c=="generation")generation();else if(c=="followers")followers();else if(c=="basis_followers")basisFollowers();else if(c=="graph")graph();else if(c=="edits")edits();else if(c=="history")history();else if(c=="masks")masks();else if(c=="serialization")serialization(argc>2?std::filesystem::path(argv[2]):std::filesystem::path{});else if(c=="atomic")atomic();else if(c=="budgets")budgets();else if(c=="evaluated_sculpt")evaluatedSculpt();else if(c=="evaluated_external")evaluatedExternal();else if(c=="evaluated_order")evaluatedOrder();else if(c=="evaluated_followers")evaluatedFollowers(false);else if(c=="evaluated_external_followers")evaluatedFollowers(true);else if(c=="evaluated_atomic")evaluatedAtomic();else if(c=="legacy_v9"&&argc==3)legacyV9(argv[2]);else return 2;
        std::cout<<c<<" passed\n";
    }
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
