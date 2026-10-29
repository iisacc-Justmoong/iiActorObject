#include <iiActorObject/SkinBinding.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DEngine.h>
#include <iiActorObject/MeshSurface.h>
#include <iiActorObject/SurfaceBinding.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#endif
#include <array>
#include <cmath>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
bool near(double a,double b){return std::abs(a-b)<1e-9;}
Object3DData data()
{
    auto d=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 3 3 3\nf 1 2 3 4\n");
    d.bones={{"root",NoBone,{}},{"left",0,{}},{"right",0,{}}};d.kind=Object3DKind::SkeletalMesh;
    Object3DEngine engine;return engine.createObject(std::move(d)).get().data();
}
void canonical()
{
    auto d=data();SkinBinder::bind(d,std::array{BoneEnvelope{1,{0,0,0},{0,1,0},.2}});
    const auto &t=*d.meshes[0].topology;CHECK(t.points.size()==5&&!t.points[0].weights.empty());
    for(const auto &p:t.points)CHECK(p.weights.size()==1&&p.weights[0].bone==1&&near(p.weights[0].weight,1));
    Object3DEngine engine;auto object=engine.createObject(d);CHECK(object.get().data().meshes[0].geometry.vertices[0].weights[0].bone==1);
}
void projection()
{
    const std::array points{Vector3{0,0,0},Vector3{1,0,0},Vector3{0,1,0}};MeshSurface surface(points,std::array<std::uint32_t,3>{0,1,2});
    const auto hit=surface.closest({.25,.25,2});CHECK(hit.valid()&&near(hit.distance,2)&&near(hit.barycentric[0],.5)&&near(hit.barycentric[1],.25)&&near(hit.barycentric[2],.25));
    auto edge=surface.closest({.75,.75,.2});CHECK(near(edge.position.x,.5)&&near(edge.position.y,.5)&&near(edge.distance,std::sqrt(.165)));
    CHECK(surface.closest({-1,-1,0}).position==Vector3(0,0,0));CHECK(!surface.closest({.25,.25,2},1).valid());CHECK(surface.closest({.5,0,0},0).valid());
    MeshSurface tie(points,std::array<std::uint32_t,6>{0,1,2,0,1,2});CHECK(tie.closest({.2,.2,1}).triangle==0);
    const std::array vertical{Vector3{0,0,0},Vector3{0,1,0},Vector3{0,0,1}};MeshSurface side(vertical,std::array<std::uint32_t,3>{0,1,2});
    const auto lateral=side.closest({2,.25,.25});CHECK(near(lateral.position.y,.25)&&near(lateral.position.z,.25)&&near(lateral.distance,2));
    rejects([&]{(void)surface.closest({0,0,0},-1);});rejects([&]{MeshSurface bad(points,std::array<std::uint32_t,3>{0,1,1});});
    std::vector<Vector3> grid;std::vector<std::uint32_t> triangles;
    for(unsigned y=0;y<=10;++y)for(unsigned x=0;x<=10;++x)grid.push_back({double(x),double(y),0});
    for(unsigned y=0;y<10;++y)for(unsigned x=0;x<10;++x){auto a=y*11+x;triangles.insert(triangles.end(),{a,a+1,a+12,a,a+12,a+11});}
    MeshSurface indexed(grid,triangles);for(unsigned i=0;i<100;++i){const auto x=.03+i*.091,y=9.7-i*.087;auto h=indexed.closest({x,y,.7});CHECK(near(h.position.x,x)&&near(h.position.y,y)&&near(h.distance,.7));}
}
double weight(const std::vector<SkinWeight> &row,BoneIndex bone){for(const auto &w:row)if(w.bone==bone)return w.weight;return 0;}
void transfer()
{
    auto source=data();auto &p=source.meshes[0].topology->points;p[0].weights={{0,1}};p[1].weights={{1,1}};p[2].weights={{1,.5},{2,.5}};p[3].weights={{2,1}};
    auto target=data();std::swap(target.bones[1].name,target.bones[2].name);auto &t=target.meshes[0].topology->points;
    t[0].position={.25,.25,.1};t[1].position={.5,.25,.1};t[2].position={.5,.5,.1};t[3].position={.25,.5,.1};
    SkinTransferOptions options;options.includeUnusedPoints=false;options.maximumDistance=.2;
    const auto report=SkinBinder::transfer(source,0,target,0,options);CHECK(report.transferred==4&&report.skipped==0&&near(report.maximumDistance,.1));
    const auto row=target.meshes[0].topology->points[0].weights;CHECK(row.size()==3&&near(weight(row,0),.5)&&near(weight(row,1),.25)&&near(weight(row,2),.25));
    options.maximumInfluences=2;(void)SkinBinder::transfer(source,0,target,0,options);CHECK(target.meshes[0].topology->points[0].weights.size()==2&&near(weight(target.meshes[0].topology->points[0].weights,0),2./3));
    options.maximumInfluences=0;auto invalid=target;invalid.meshes[0].topology->points[2].position.z=2;const auto original=invalid.meshes[0].topology->points[0].weights;
    rejects([&]{(void)SkinBinder::transfer(source,0,invalid,0,options);});CHECK(near(weight(invalid.meshes[0].topology->points[0].weights,0),weight(original,0)));
    options.skipUnmatched=true;const auto skipped=SkinBinder::transfer(source,0,invalid,0,options);CHECK(skipped.transferred==3&&skipped.skipped==1);
    target.bones[0].name="other";rejects([&]{(void)SkinBinder::transfer(source,0,target,0,options);});
    options.sourceToTarget={0,2,1};CHECK(SkinBinder::transfer(source,0,target,0,options).transferred==4);

    // Skin influences and DQ factors must use the same source triangle and hit.
    auto from=data(),to=data();from.meshes[0].topology->skinning={SkinningMethod::Blended,{0,.25,.75,.5,.9}};
    to.meshes[0].topology->skinning={SkinningMethod::DualQuaternion,{}};
    auto &dest=to.meshes[0].topology->points;
    dest[0].position={.25,.25,.1};dest[1].position={.5,.25,.1};dest[2].position={.5,.5,.1};dest[3].position={.25,.5,.1};
    SkinTransferOptions settings;settings.includeUnusedPoints=false;settings.maximumDistance=.2;
    auto legacy=to;(void)SkinBinder::transfer(from,0,legacy,0,settings);
    CHECK(legacy.meshes[0].topology->skinning.method==SkinningMethod::DualQuaternion);
    auto copied=to;const auto projected=SkinBinder::transfer(from,0,copied,0,settings,SkinningProfileTransfer::Source);
    const auto &profile=copied.meshes[0].topology->skinning;
    CHECK(projected.transferred==4&&profile.method==SkinningMethod::Blended&&profile.blendWeights.size()==5);
    CHECK(near(profile.blendWeights[0],.1875)&&near(profile.blendWeights[1],.25)&&near(profile.blendWeights[2],.375)&&near(profile.blendWeights[3],.3125)&&near(profile.blendWeights[4],1));
    const auto &layout=copied.meshes[0].topology->layout->definition();
    for(std::size_t i=0;i<layout.renderToLayout.size();++i)
        CHECK(near(copied.meshes[0].geometry.skinning.blendWeights[i],profile.blendWeights[layout.layouts[layout.renderToLayout[i]].position]));
    auto failed=to;failed.meshes[0].topology->points[2].position.z=2;
    const auto before=failed.meshes[0].topology->skinning;const auto rowBefore=failed.meshes[0].topology->points[0].weights;
    rejects([&]{(void)SkinBinder::transfer(from,0,failed,0,settings,SkinningProfileTransfer::Source);});
    CHECK(failed.meshes[0].topology->skinning==before&&near(weight(failed.meshes[0].topology->points[0].weights,0),weight(rowBefore,0)));
    settings.skipUnmatched=true;(void)SkinBinder::transfer(from,0,failed,0,settings,SkinningProfileTransfer::Source);
    CHECK(near(failed.meshes[0].topology->skinning.blendWeights[2],1)&&near(failed.meshes[0].topology->skinning.blendWeights[4],1));
    auto full=to;from.meshes[0].topology->skinning={SkinningMethod::DualQuaternion,{}};
    settings.includeUnusedPoints=true;settings.maximumDistance=std::numeric_limits<double>::infinity();settings.skipUnmatched=false;
    (void)SkinBinder::transfer(from,0,full,0,settings,SkinningProfileTransfer::Source);
    CHECK(full.meshes[0].topology->skinning.method==SkinningMethod::DualQuaternion);
    from.meshes[0].topology->skinning={};
    auto preserve=to;(void)SkinBinder::transfer(from,0,preserve,0,settings,SkinningProfileTransfer::SourceIfNonlinear);
    CHECK(preserve.meshes[0].topology->skinning.method==SkinningMethod::DualQuaternion);
    (void)SkinBinder::transfer(from,0,preserve,0,settings,SkinningProfileTransfer::Source);
    CHECK(preserve.meshes[0].topology->skinning.method==SkinningMethod::Linear);
    rejects([&]{(void)SkinBinder::transfer(from,0,preserve,0,settings,static_cast<SkinningProfileTransfer>(255));});

    Object3DEngine owner;const auto sourceAsset=owner.createObject(from);auto editor=owner.editObject(owner.createObject(to));
    (void)editor.get().transferWeights(0,sourceAsset.get(),0,settings,SkinningProfileTransfer::Source);
    CHECK(editor.get().object().get().data().meshes[0].topology->skinning.method==SkinningMethod::Linear);
    CHECK(editor.get().undo()&&editor.get().object().get().data().meshes[0].topology->skinning.method==SkinningMethod::DualQuaternion);
    CHECK(editor.get().redo());Object3DEngine restored;
    const auto project=importObject3DProject(restored.reference(),exportObject3DProject(editor));
    CHECK(project.editor.get().undo()&&project.editor.get().object().get().data().meshes[0].topology->skinning.method==SkinningMethod::DualQuaternion);
    Object3DTemplateDefinition definition;definition.base=owner.createObject(to);
    auto layered=owner.editTemplate(owner.createTemplate(std::move(definition)));
    (void)layered.get().transferWeights(0,sourceAsset.get(),0,settings,SkinningProfileTransfer::Source);
    CHECK(layered.get().layers().size()==1&&layered.get().object().get().data().meshes[0].topology->skinning.method==SkinningMethod::Linear);
    const auto layer=layered.get().layers()[0].id;layered.get().setLayerEnabled(layer,false);
    CHECK(layered.get().object().get().data().meshes[0].topology->skinning.method==SkinningMethod::DualQuaternion);
#if IIACTOROBJECT_HAS_NSCHARACTER
    NSCharacter::CharacterAsset sourceView(sourceAsset);NSCharacter::CharacterEditor character(layered);
    (void)character.transferWeights(0,sourceView,0,settings,SkinningProfileTransfer::Source);
    CHECK(character.asset()->data().meshes[0].topology->skinning.method==SkinningMethod::Linear);
#endif
}
void mirror()
{
    auto d=data();auto &p=d.meshes[0].topology->points;
    p[0].position.x=-1;p[3].position.x=-1;p[0].weights=p[3].weights={{1,1}};p[1].weights=p[2].weights={{0,1}};
    SkinMirrorOptions options;options.transfer.includeUnusedPoints=false;options.transfer.maximumDistance=.01;
    const auto result=SkinBinder::mirror(d,0,std::array<BoneIndex,3>{0,2,1},options);CHECK(result.transferred==2);
    CHECK(near(weight(d.meshes[0].topology->points[1].weights,2),1));CHECK(near(weight(d.meshes[0].topology->points[0].weights,1),1));
    rejects([&]{(void)SkinBinder::mirror(d,0,std::array<BoneIndex,3>{0,2,2},options);});
    auto missing=data();options.transfer.maximumDistance=std::numeric_limits<double>::infinity();
    rejects([&]{(void)SkinBinder::mirror(missing,0,std::array<BoneIndex,3>{0,2,1},options);});
    auto profiled=data();auto &points=profiled.meshes[0].topology->points;
    points[0].position.x=-1;points[3].position.x=-1;points[1].position.x=.25;points[2].position.x=.5;
    profiled.meshes[0].topology->skinning={SkinningMethod::Blended,{.2,.45,.7,.4,.9}};
    const auto original=profiled;
    options.transfer.maximumDistance=.01;const auto mirrored=SkinBinder::mirror(profiled,0,std::array<BoneIndex,3>{0,2,1},options,SkinningProfileTransfer::Source);
    const auto &amount=profiled.meshes[0].topology->skinning.blendWeights;
    CHECK(mirrored.transferred==2&&near(amount[0],.2)&&near(amount[1],.35)&&near(amount[2],.5)&&near(amount[3],.4)&&near(amount[4],.9));
    Object3DEngine engine;auto edit=engine.editObject(engine.createObject(original));
    (void)edit.get().mirrorWeights(0,std::array<BoneIndex,3>{0,2,1},options,SkinningProfileTransfer::Source);
    CHECK(near(edit.get().object().get().data().meshes[0].topology->skinning.blendWeights[1],.35));
    CHECK(edit.get().undo()&&near(edit.get().object().get().data().meshes[0].topology->skinning.blendWeights[1],.45));
#if IIACTOROBJECT_HAS_NSCHARACTER
    NSCharacter::CharacterEditor character(edit);
    (void)character.mirrorWeights(0,std::array<BoneIndex,3>{0,2,1},options,SkinningProfileTransfer::Source);
    CHECK(near(character.asset()->data().meshes[0].topology->skinning.blendWeights[2],.5));
#endif
}
Object3DData clothing()
{
    auto d=data();auto cloth=d.meshes[0];cloth.name="Cloth";for(auto &p:cloth.topology->points)p.position.z+=.1;d.meshes.push_back(std::move(cloth));return d;
}
void conform()
{
    const auto base=clothing();const auto binding=bindMeshSurface(base,0,1,{.2,false});CHECK(binding.points.size()==4);
    SurfaceBindingOptions bounded;SurfaceBindingLimits limits;const std::array<std::uint32_t,1> hidden{4};limits.additionalPoints=hidden;
    CHECK(bindMeshSurface(base,0,1,bounded,limits).points.size()==5);
    auto bad=limits;bad.maximumVertices=3;rejects([&]{(void)bindMeshSurface(base,0,1,bounded,bad);});
    bad=limits;bad.maximumTriangles=1;rejects([&]{(void)bindMeshSurface(base,0,1,bounded,bad);});
    bad=limits;bad.maximumPoints=4;rejects([&]{(void)bindMeshSurface(base,0,1,bounded,bad);});
    bad=limits;bad.maximumNodeVisits=1;rejects([&]{(void)bindMeshSurface(base,0,1,bounded,bad);});
    bad=limits;bad.maximumTriangleTests=1;rejects([&]{(void)bindMeshSurface(base,0,1,bounded,bad);});
    const std::array<std::uint32_t,1> missing{99};bad=limits;bad.additionalPoints=missing;rejects([&]{(void)bindMeshSurface(base,0,1,bounded,bad);});
    auto d=base;for(unsigned i=0;i<4;++i)d.meshes[0].topology->points[i].position.z+=.2;
    applyMeshSurfaceBindings(d,base,std::span(&binding,1));CHECK(near(d.meshes[1].topology->points[0].position.z,.3));CHECK(near(d.meshes[1].geometry.vertices[0].position.z,.3));
    CHECK(d.meshes[1].topology->points[4].position==base.meshes[1].topology->points[4].position);
    d=base;for(unsigned i=0;i<4;++i){auto &p=d.meshes[0].topology->points[i].position;p={0,p.y,-p.x};}
    applyMeshSurfaceBindings(d,base,std::span(&binding,1));CHECK(near(d.meshes[1].topology->points[0].position.x,.1)&&near(d.meshes[1].topology->points[0].position.z,0));
    auto chain=base;auto coat=chain.meshes[1];for(auto &p:coat.topology->points)p.position.z+=.1;chain.meshes.push_back(std::move(coat));
    const std::array bindings{bindMeshSurface(chain,1,2),bindMeshSurface(chain,0,1)};auto next=chain;for(unsigned i=0;i<4;++i)next.meshes[0].topology->points[i].position.z+=.2;
    applyMeshSurfaceBindings(next,chain,bindings);CHECK(near(next.meshes[2].topology->points[0].position.z,.4));
    auto invalid=binding;invalid.points[0].barycentric[0]=2;rejects([&]{applyMeshSurfaceBindings(d,base,std::span(&invalid,1));});
    const std::array cycle{binding,bindMeshSurface(base,1,0)};rejects([&]{validateMeshSurfaceBindings(base,cycle);});
}
void layers()
{
    Object3DEngine engine;auto d=clothing();SkinTransferOptions weights;weights.includeUnusedPoints=false;(void)SkinBinder::transfer(d,0,d,1,weights);
    Object3DTemplateDefinition definition;definition.surfaces={bindMeshSurface(d,0,1)};definition.base=engine.createObject(d);
    definition.targets={{"height",0,{{2,{0,.2,0}},{3,{0,.2,0}}}},{"flare",1,{{2,{.1,0,0}}}}};auto editor=engine.editTemplate(engine.createTemplate(std::move(definition)),std::array{TemplateValue{"height",.5}});
    CHECK(near(editor.get().object().get().data().meshes[1].topology->points[2].position.y,1.1));
    const std::array moves{TopologyPointMove{0,{0,0,.02}},TopologyPointMove{1,{0,0,.02}},TopologyPointMove{2,{0,0,.02}},TopologyPointMove{3,{0,0,.02}}};
    editor.get().moveControlPoints(0,moves);editor.get().moveControlPoints(1,std::array{TopologyPointMove{0,{0,0,.03}}});
    editor.get().setTemplateValues(std::array{TemplateValue{"height",1},TemplateValue{"flare",.5}});
    CHECK(near(editor.get().object().get().data().meshes[1].topology->points[0].position.z,.15));CHECK(near(editor.get().object().get().data().meshes[1].topology->points[2].position.x,1.05));
    Object3DEngine restored;auto project=importObject3DProject(restored.reference(),exportObject3DProject(editor));
    project.editor.get().setTemplateValues(std::array{TemplateValue{"height",.25}});
    CHECK(near(project.editor.get().object().get().data().meshes[1].topology->points[0].position.z,.15));CHECK(near(project.editor.get().object().get().data().meshes[1].topology->points[2].position.y,1.05));
    CHECK(project.editor.get().undo());CHECK(project.editor.get().redo());
}
void attach()
{
    Object3DEngine engine;Object3DTemplateDefinition definition;definition.base=engine.createObject(data());definition.targets={{"height",0,{{2,{0,.2,0}},{3,{0,.2,0}}}}};
    auto editor=engine.editTemplate(engine.createTemplate(std::move(definition)),std::array{TemplateValue{"height",.5}});
    const std::array moves{TopologyPointMove{0,{0,0,.02}},TopologyPointMove{1,{0,0,.02}},TopologyPointMove{2,{0,0,.02}},TopologyPointMove{3,{0,0,.02}}};editor.get().moveControlPoints(0,moves);
    auto cloth=editor.get().object().get().data().meshes[0];cloth.name="Cloth";for(auto &p:cloth.topology->points)p.position.z+=.1;cloth.topology->points[4].weights={{999,1}};
    SurfaceAttachmentOptions invalid;invalid.binding.maximumDistance=.001;const auto revision=editor.get().revision();
    rejects([&]{(void)editor.get().attachSurface(cloth,0,invalid);});CHECK(editor.get().revision()==revision&&editor.get().object().get().data().meshes.size()==1);
    const auto attached=editor.get().attachSurface(cloth,0);CHECK(attached.mesh==1&&attached.skin.transferred==4);
    CHECK(editor.get().object().get().data().meshes[1].topology->points[4].weights.empty());
    CHECK(near(editor.get().object().get().data().meshes[1].topology->points[0].position.z,.12));
    editor.get().moveControlPoints(1,std::array{TopologyPointMove{0,{0,0,.03}}});editor.get().setTemplateValues(std::array{TemplateValue{"height",1}});
    CHECK(near(editor.get().object().get().data().meshes[1].topology->points[0].position.z,.15));
    CHECK(editor.get().undo()&&editor.get().undo()&&editor.get().undo());CHECK(editor.get().object().get().data().meshes.size()==1);
    Object3DEngine restored;auto project=importObject3DProject(restored.reference(),exportObject3DProject(editor));
    CHECK(project.editor.get().redo()&&project.editor.get().object().get().data().meshes.size()==2);CHECK(project.editor.get().redo()&&project.editor.get().redo());
    CHECK(near(project.editor.get().object().get().data().meshes[1].topology->points[0].position.z,.15));
    CHECK(near(project.editor.get().object().get().data().meshes[1].topology->points[2].position.y,1.2));CHECK(project.editor.get().templateProgram().get().definition().surfaces.size()==1);
}
#if IIACTOROBJECT_HAS_NSCHARACTER
void human(const std::filesystem::path &directory,const std::filesystem::path &output)
{
    Object3DEngine engine;auto human=NSCharacter::loadMakeHumanTemplate(engine.reference(),directory);auto definition=human.program().get().definition();auto neutral=definition.base.get().data();
    neutral.materials.push_back({"Garment",{.1,.25,.8,1},0,.8});definition.base=engine.createObject(neutral);
    const auto &base=*neutral.meshes[0].topology;std::vector<bool> region(base.points.size(),false);
    for(std::size_t i=0;i<base.points.size();++i){const auto p=base.points[i].position;region[i]=p.y>.95&&p.y<1.45&&std::abs(p.x)<.4;}
    std::string parameter;double largest=0;for(const auto &target:definition.targets)for(const auto &delta:target.deltas)if(target.mesh==0&&region[delta.point])
    {const auto size=std::hypot(delta.position.x,delta.position.y,delta.position.z);if(size>largest){largest=size;parameter=target.name;}}
    CHECK(largest>.01&&!parameter.empty());auto program=engine.createTemplate(std::move(definition));NSCharacter::CharacterEditor editor(engine.editTemplate(program,std::array{TemplateValue{parameter,.25}},8));
    const auto &source=editor.asset()->data().meshes[0];const auto &t=*source.topology;auto cloth=source;cloth.name="GarmentProxy";cloth.material=1;cloth.topology->morphTargets.clear();
    auto layout=t.layout->definition();std::vector<bool> hidden(layout.polygons.size(),false);for(auto id:layout.hiddenPolygons)hidden[id]=true;layout.hiddenPolygons.clear();layout.renderToLayout.clear();layout.sourceFormat="surface-proxy";layout.sourceElement=NoTopologyIndex;
    for(std::size_t f=0;f<layout.polygons.size();++f)
    {
        Vector3 centre{};for(auto id:layout.polygons[f]){const auto p=t.points[layout.layouts[id].position].position;centre.x+=p.x;centre.y+=p.y;centre.z+=p.z;}
        const auto count=static_cast<double>(layout.polygons[f].size());centre={centre.x/count,centre.y/count,centre.z/count};
        if(hidden[f]||centre.y<.95||centre.y>1.45||std::abs(centre.x)>.4)layout.hiddenPolygons.push_back(static_cast<std::uint32_t>(f));
    }
    layout.polygonMaterials.assign(layout.polygons.size(),1);std::vector<Vector3> normals(t.points.size());
    const auto &sourceLayout=t.layout->definition();for(std::size_t r=0;r<sourceLayout.renderToLayout.size();++r)
    {auto &n=normals[sourceLayout.layouts[sourceLayout.renderToLayout[r]].position];const auto v=source.geometry.vertices[r].normal;n.x+=v.x;n.y+=v.y;n.z+=v.z;}
    for(std::size_t i=0;i<normals.size();++i){const auto n=normals[i];const auto length=std::hypot(n.x,n.y,n.z);if(length>0){auto &p=cloth.topology->points[i].position;p.x+=.006*n.x/length;p.y+=.006*n.y/length;p.z+=.006*n.z/length;}}
    cloth.topology->layout=MeshTopologyLayout::create(std::move(layout),cloth.topology->points);
    SurfaceAttachmentOptions options;options.binding.maximumDistance=.03;options.skin.maximumDistance=.03;
    const auto attached=editor.attachSurface(std::move(cloth),0,options);CHECK(attached.mesh==1&&attached.skin.transferred>500&&attached.skin.skipped==0);
    const auto before=editor.asset();editor.setTemplateValues(std::array{TemplateValue{parameter,.75}});auto after=editor.asset();
    const auto &a=after->data();const auto &garment=*a.meshes[1].topology;std::vector<Vector3> body;for(const auto &v:a.meshes[0].geometry.vertices)body.push_back(v.position);MeshSurface bodySurface(body,a.meshes[0].geometry.triangles);
    double change=0,gap=0;std::size_t influences=0;const auto &binding=editor.session().get().templateProgram().get().definition().surfaces[0];
    for(const auto &p:binding.points)
    {
        const auto x=garment.points[p.point].position,y=before->data().meshes[1].topology->points[p.point].position;change=std::max(change,std::hypot(x.x-y.x,x.y-y.y,x.z-y.z));
        gap=std::max(gap,bodySurface.closest(x).distance);influences=std::max(influences,garment.points[p.point].weights.size());
    }
    CHECK(change>.005&&gap<.01&&a.bones.size()==163);
    Pose pose(after->skeleton());const auto bone=after->skeleton()->findBone("spine02");CHECK(bone!=NoBone);auto transform=pose.locals()[bone];const auto q=transform.rotation,r=Quaternion::fromAxisAngle({0,0,1},.25);
    transform.rotation={q.w*r.x+q.x*r.w+q.y*r.z-q.z*r.y,q.w*r.y-q.x*r.z+q.y*r.w+q.z*r.x,q.w*r.z+q.x*r.y-q.y*r.x+q.z*r.w,q.w*r.w-q.x*r.x-q.y*r.y-q.z*r.z};pose.setLocal(bone,transform);
    std::vector<DeformedVertex> posed(a.meshes[1].geometry.vertices.size());after->meshes()[1]->deform(pose,0,{},posed);double skin=0;
    for(std::size_t i=0;i<posed.size();++i){const auto p=posed[i].position,r=a.meshes[1].geometry.vertices[i].position;skin=std::max(skin,std::hypot(p.x-r.x,p.y-r.y,p.z-r.z));}CHECK(skin>.01);
    std::filesystem::create_directories(output);auto save=[&](const auto &path,const auto &bytes){std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(file.good());};
    save(output/"ClothedHuman.glb",exportObject3DGlb(after->object().get()));save(output/"ClothedHuman.fbx",exportObject3DFbx(after->object().get()));
    save(output/"ClothedHuman.ii3dproject",exportObject3DProject(editor.session()));
    Object3DEngine restored;std::ifstream file(output/"ClothedHuman.ii3dproject",std::ios::binary);std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});auto project=importObject3DProject(restored.reference(),bytes);
    project.editor.get().setTemplateValues(std::array{TemplateValue{parameter,.5}});CHECK(project.editor.get().object().get().data().meshes.size()==2);
    std::cout<<"surfacePoints="<<attached.skin.transferred<<" parameter="<<parameter<<" conformChange="<<change<<" maxSurfaceGap="<<gap<<" skinChange="<<skin<<" maxInfluences="<<influences<<'\n';
}
#endif
int main(int argc,char**argv)
{
    try{if(argc<2)return 2;const std::string_view mode=argv[1];if(mode=="canonical")canonical();else if(mode=="projection")projection();else if(mode=="transfer")transfer();else if(mode=="mirror")mirror();else if(mode=="conform")conform();else if(mode=="layers")layers();else if(mode=="attach")attach();
#if IIACTOROBJECT_HAS_NSCHARACTER
    else if(mode=="human"&&argc==4)human(argv[2],argv[3]);
#endif
    else return 2;std::cout<<mode<<" passed\n";return 0;}
    catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
