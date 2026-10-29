#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DProxy.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>

using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
bool near(double a,double b){return std::abs(a-b)<1e-10;}
void writeFixture(const char *path,const std::vector<std::uint8_t> &bytes)
{
    if(!path||!*path||std::string_view(path)=="-")return;
    std::ofstream fixture(path,std::ios::binary|std::ios::trunc);
    fixture.write(reinterpret_cast<const char *>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    if(!fixture)throw std::runtime_error("Could not write surface fixture");
}
void repairChecksum(std::vector<std::uint8_t> &bytes)
{
    std::uint32_t crc=0xffffffffU;
    for(std::size_t i=24;i<bytes.size();++i)
    {
        crc^=bytes[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320U:0);
    }
    crc^=0xffffffffU;for(unsigned i=0;i<4;++i)bytes[20+i]=static_cast<std::uint8_t>(crc>>(8*i));
}

Object3DTemplateHandle makeTemplate(Object3DEngine &engine,bool alternative,bool covered=false,bool grounding=false,bool unsafeGround=false)
{
    auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 2 0 0\nv 0 1 0\nv 1 1 .1\nv 2 1 0\nf 1 2 5 4\nf 2 3 6 5\n");
    data.bones={{"root",NoBone,{}},{"child",0,{}}};data.kind=Object3DKind::SkeletalMesh;
    for(auto &point:data.meshes[0].topology->points)point.weights={{0,1}};
    if(covered)(void)hideObject3DProxyCoveredPolygons(*data.meshes[0].topology,std::array<std::uint32_t,4>{0,1,3,4});
    auto other=parseObject3DObj(alternative?"v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n":"v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    auto mesh=other.meshes[0];mesh.name=alternative?"New surface":"Old surface";mesh.material=1;
    Object3DMaterial material;material.name=alternative?"New material":"Old material";data.materials.push_back(material);
    data.meshes.push_back(std::move(mesh));
    const std::array<std::uint32_t,4> source{0,1,4,3};std::vector<SurfaceControlPoint> points;
    for(std::size_t i=0;i<data.meshes[1].topology->points.size();++i)
        points.push_back({static_cast<std::uint32_t>(i),{source[i],source[i],source[i]},{1,0,0},{0,0,alternative?.2:.1}});
    const auto binding=bindMeshControlPoints(data,0,1,points);fitMeshSurfaceBinding(data,binding);(void)transferMeshBindingWeights(data,binding);
    Object3DTemplateDefinition definition;definition.base=engine.createObject(std::move(data));definition.surfaces={binding};
    definition.targets={{"height",0,{{4,{0,.2,0}}}}};
    if(unsafeGround)definition.targets[0].deltas.push_back({0,{0,-.2,0}});
    if(grounding)definition.grounding=TemplateGrounding{};
    return engine.createTemplate(std::move(definition));
}
void swapping(const char *fixturePath=nullptr)
{
    Object3DEngine engine;const auto original=makeTemplate(engine,false),alternative=makeTemplate(engine,true,true);
    auto editor=engine.editTemplate(original,std::array{TemplateValue{"height",.5}});
    editor.get().moveControlPoints(0,std::array{TopologyPointMove{4,{0,0,.03}}});
    editor.get().setControlWeights(0,std::array{TopologyPointWeights{4,{{1,1}}}});
    editor.get().setSkinningProfile(0,SkinningProfile{SkinningMethod::DualQuaternion,{}});
    editor.get().renameJoint(1,"RenamedChild");
    editor.get().setPointSelection("Body",std::array{MeshPointMask{0,{0,0,0,0,1,0}}});
    const auto previous=editor.get().object();const auto revision=editor.get().revision();
    CHECK(editor.get().replaceTemplateSurfaces(alternative,std::array<std::size_t,1>{1},std::array<std::size_t,1>{0}));
    const auto &result=editor.get().object().get().data();
    CHECK(editor.get().revision()==revision+1&&result.meshes[1].name=="New surface");
    CHECK(result.meshes[0].topology->layout->definition().hiddenPolygons==std::vector<std::uint32_t>({0}));
    CHECK(near(result.meshes[0].topology->points[4].position.y,1.1)&&near(result.meshes[0].topology->points[4].position.z,.13));
    CHECK(near(result.meshes[1].topology->points[2].position.y,1.1)&&near(result.meshes[1].topology->points[2].position.z,.33));
    CHECK(result.meshes[1].topology->points[2].weights.size()==1&&result.meshes[1].topology->points[2].weights[0].bone==1);
    CHECK(result.meshes[1].topology->skinning.method==SkinningMethod::DualQuaternion);
    CHECK(result.bones[1].name=="RenamedChild");
    CHECK(editor.get().pointSelection("Body")[0].weights[4]==1);
    CHECK(editor.get().undo()&&editor.get().object().get().data().meshes[1].name=="Old surface");
    CHECK(editor.get().redo()&&editor.get().object().get().data().meshes[1].name=="New surface");
    editor.get().setTemplateValues(std::array{TemplateValue{"height",1}});
    CHECK(near(editor.get().object().get().data().meshes[1].topology->points[2].position.y,1.2));
    CHECK(editor.get().object().get().data().meshes[1].topology->points[2].weights[0].bone==1);
    CHECK(editor.get().undo());
    const auto bytes=exportObject3DProject(editor);writeFixture(fixturePath,bytes);
    Object3DEngine restored;auto project=importObject3DProject(restored.reference(),bytes);
    CHECK(project.editor.get().undo()&&project.editor.get().object().get().data().meshes[1].name=="Old surface");
    CHECK(project.editor.get().redo()&&project.editor.get().object().get().data().meshes[1].name=="New surface");
    CHECK(project.editor.get().pointSelection("Body")[0].weights[4]==1&&previous.valid());
}
void safeguards()
{
    Object3DEngine engine;const auto original=makeTemplate(engine,false),alternative=makeTemplate(engine,true,true);
    auto editor=engine.editTemplate(original);editor.get().moveControlPoints(1,std::array{TopologyPointMove{0,{0,0,.01}}});
    const auto revision=editor.get().revision();const auto objects=engine.objectCount(),programs=engine.reference().templateCount();
    rejects([&]{(void)editor.get().replaceTemplateSurfaces(alternative,std::array<std::size_t,1>{1},std::array<std::size_t,1>{0});});
    CHECK(editor.get().revision()==revision&&engine.objectCount()==objects&&engine.reference().templateCount()==programs);
    CHECK(editor.get().undo());
    editor.get().setPointSelection("Accessory",std::array{MeshPointMask{1,{1,0,0}}});
    rejects([&]{(void)editor.get().replaceTemplateSurfaces(alternative,std::array<std::size_t,1>{1},std::array<std::size_t,1>{0});});
    editor.get().removePointSelection("Accessory");
    auto changed=editor.get().object().get().data();changed.meshes[0].material=1;editor.get().replace(std::move(changed),"Share material");
    rejects([&]{(void)editor.get().replaceTemplateSurfaces(alternative,std::array<std::size_t,1>{1},std::array<std::size_t,1>{0});});
    CHECK(editor.get().undo());
    auto material=editor.get().object().get().data().materials[1].surface;material.doubleSided=true;
    CHECK(editor.get().setMaterialSurface(1,material));
    rejects([&]{(void)editor.get().replaceTemplateSurfaces(alternative,std::array<std::size_t,1>{1},std::array<std::size_t,1>{0});});
    CHECK(editor.get().undo());
    Object3DEngine foreign;auto other=makeTemplate(foreign,true);
    rejects([&]{(void)editor.get().replaceTemplateSurfaces(other,std::array<std::size_t,1>{1},std::array<std::size_t,1>{0});});
    rejects([&]{(void)editor.get().replaceTemplateSurfaces(alternative,std::array<std::size_t,2>{1,1},std::array<std::size_t,1>{0});});
}
void replacementResources(const char *fixturePath,const char *firstPath,const char *secondPath)
{
    const auto read=[](const char *path)
    {
        std::ifstream input(path,std::ios::binary);
        if(!input)throw std::runtime_error("Missing replacement resource fixture");
        return std::vector<std::uint8_t>{std::istreambuf_iterator<char>(input),{}};
    };
    const auto blue=read(firstPath),checker=read(secondPath);
    Object3DEngine engine;
    const auto create=[&](bool alternative)
    {
        const auto original=makeTemplate(engine,alternative);
        auto definition=original.get().definition();auto data=definition.base.get().data();
        data.images.emplace_back(alternative?"Unused":"Old color",blue);
        if(alternative)data.images.emplace_back("New color",checker);
        data.materials[1].baseColorImage=alternative?1:0;
        definition.base=engine.createObject(std::move(data));
        return engine.createTemplate(std::move(definition));
    };
    const auto original=create(false),alternative=create(true);auto editor=engine.editTemplate(original);
    auto bodySurface=editor.get().object().get().data().materials[0].surface;bodySurface.doubleSided=true;
    CHECK(editor.get().setMaterialSurface(0,bodySurface));
    CHECK(editor.get().replaceTemplateSurfaces(alternative,std::array<std::size_t,1>{1}));
    const auto &result=editor.get().object().get().data();
    CHECK(result.materials[0].surface.doubleSided&&result.materials[1].baseColorImage==0);
    CHECK(result.materials[1].name=="New material"&&result.images[0].name()=="New color");
    CHECK(result.images[0]==alternative.get().definition().base.get().data().images[1]);
    CHECK(editor.get().undo());
    auto badDefinition=alternative.get().definition();auto bad=badDefinition.base.get().data();
    bad.materials[0].baseColorImage=1;badDefinition.base=engine.createObject(std::move(bad));
    const auto shared=engine.createTemplate(std::move(badDefinition));
    const auto revision=editor.get().revision();const auto objects=engine.objectCount(),programs=engine.templateCount();
    rejects([&]{(void)editor.get().replaceTemplateSurfaces(shared,std::array<std::size_t,1>{1});});
    CHECK(editor.get().revision()==revision&&engine.objectCount()==objects&&engine.templateCount()==programs);
    CHECK(editor.get().redo()&&editor.get().object().get().data().images[0].name()=="New color");
    const auto encoded=exportObject3DProject(editor);writeFixture(fixturePath,encoded);
    Object3DEngine restored;auto project=importObject3DProject(restored.reference(),encoded);
    CHECK(project.editor.get().object().get().data().images[0].name()=="New color");
}
std::size_t archiveSize(bool large,unsigned swaps)
{
    constexpr std::size_t count=600;Object3DEngine engine;
    const auto make=[&](bool alternative)
    {
        std::ostringstream obj;
        for(std::size_t i=0;i<count/3;++i)
            obj<<"v "<<i<<" 0 0\nv "<<i<<".5 0 0\nv "<<i<<" 1 0\n";
        for(std::size_t i=0;i<count/3;++i)obj<<"f "<<3*i+1<<' '<<3*i+2<<' '<<3*i+3<<'\n';
        auto data=parseObject3DObj(obj.str());data.bones={{"root",NoBone,{}}};
        for(auto &point:data.meshes[0].topology->points)point.weights={{0,1}};
        auto part=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n").meshes[0];
        part.name=alternative?"B":"A";part.material=1;data.meshes.push_back(std::move(part));
        Object3DMaterial material;material.name=alternative?"BMaterial":"AMaterial";data.materials.push_back(material);
        const std::array rows{SurfaceControlPoint{0,{0,0,0},{1,0,0},{0,0,.1}},SurfaceControlPoint{1,{1,1,1},{1,0,0},{0,0,.1}},SurfaceControlPoint{2,{2,2,2},{1,0,0},{0,0,.1}}};
        auto binding=bindMeshControlPoints(data,0,1,rows);fitMeshSurfaceBinding(data,binding);(void)transferMeshBindingWeights(data,binding);
        Object3DTemplateDefinition definition;definition.base=engine.createObject(std::move(data));definition.surfaces={std::move(binding)};
        return engine.createTemplate(std::move(definition));
    };
    const auto a=make(false),b=make(true);auto editor=engine.editTemplate(a,{},16);
    std::vector<TopologyPointMove> moves;for(std::size_t i=0;i<(large?count:1);++i)moves.push_back({static_cast<std::uint32_t>(i),{0,0,.01}});
    editor.get().moveControlPoints(0,moves);
    for(unsigned i=0;i<swaps;++i)CHECK(editor.get().replaceTemplateSurfaces(i%2? a:b,std::array<std::size_t,1>{1}));
    return exportObject3DProject(editor).size();
}
void sharing()
{
    const auto small=archiveSize(false,0),large=archiveSize(true,0);
    const auto smallHistory=archiveSize(false,6),largeHistory=archiveSize(true,6);
    CHECK(large>small&&largeHistory>smallHistory);
    const auto initial=large-small,afterSwaps=largeHistory-smallHistory;
    CHECK(afterSwaps<2*initial);
    std::cout<<"initialEditBytes="<<initial<<" sixSwapEditBytes="<<afterSwaps<<'\n';
}
void grounding()
{
    Object3DEngine engine;const auto safe=makeTemplate(engine,false,false,true),safeAlternative=makeTemplate(engine,true,true,true);
    auto editor=engine.editTemplate(safe);const auto before=editor.get().object().get().data().meshes[0].topology->points[4].position;
    CHECK(editor.get().replaceTemplateSurfaces(safeAlternative,std::array<std::size_t,1>{1},std::array<std::size_t,1>{0}));
    CHECK(near(editor.get().object().get().data().meshes[0].topology->points[4].position.y,before.y));
    CHECK(!editor.get().templateProgram().get().definition().grounding->supportPoints.empty());
    editor.get().setTemplateValues(std::array{TemplateValue{"height",1}});
    CHECK(near(editor.get().object().get().data().meshes[0].topology->points[4].position.y,before.y+.2));
    const auto unsafe=makeTemplate(engine,false,false,true,true),unsafeAlternative=makeTemplate(engine,true,true,true,true);
    const std::array values{TemplateValue{"height",1}};const auto expected=unsafe.get().snapshot(values);
    auto unsafeEditor=engine.editTemplate(unsafe);const auto revision=unsafeEditor.get().revision();
    CHECK(unsafeEditor.get().replaceTemplateSurfaces(unsafeAlternative,std::array<std::size_t,1>{1},std::array<std::size_t,1>{0}));
    CHECK(unsafeEditor.get().revision()==revision+1&&unsafeEditor.get().undo()&&unsafeEditor.get().redo());
    unsafeEditor.get().setTemplateValues(values);
    for(std::size_t i=0;i<expected.meshes[0].topology->points.size();++i)
        CHECK(near(unsafeEditor.get().object().get().data().meshes[0].topology->points[i].position.y,expected.meshes[0].topology->points[i].position.y));
    const auto bytes=exportObject3DProject(unsafeEditor);CHECK(bytes[8]==32);
    Object3DEngine destination;auto restored=importObject3DProject(destination.reference(),bytes);
    CHECK(restored.editor.get().templateProgram().get().definition().grounding->supportPoints==unsafeEditor.get().templateProgram().get().definition().grounding->supportPoints);
    restored.editor.get().setTemplateValues(std::array{TemplateValue{"height",.5}});
    const auto half=unsafe.get().snapshot(std::array{TemplateValue{"height",.5}});
    CHECK(near(restored.editor.get().object().get().data().meshes[0].topology->points[4].position.y,half.meshes[0].topology->points[4].position.y));
    const auto covered=makeTemplate(engine,false,true,true,true),uncovered=makeTemplate(engine,true,false,true,true);
    const auto coveredExpected=covered.get().snapshot(values);auto revealing=engine.editTemplate(covered);
    CHECK(revealing.get().replaceTemplateSurfaces(uncovered,std::array<std::size_t,1>{1},std::array<std::size_t,1>{0}));
    revealing.get().setTemplateValues(values);
    for(std::size_t i=0;i<coveredExpected.meshes[0].topology->points.size();++i)
        CHECK(near(revealing.get().object().get().data().meshes[0].topology->points[i].position.y,coveredExpected.meshes[0].topology->points[i].position.y));
}
Object3DTemplateHandle frameTemplate(Object3DEngine &engine,bool hidden,bool firstFace,bool extra=false)
{
    auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 2 0 0\nv 0 1 0\nv 1 1 0\nv 2 1 0\nf 1 2 5 4\nf 2 3 6 5\n");
    data.bones={{"root",NoBone,{}}};for(auto &point:data.meshes[0].topology->points)point.weights={{0,1}};
    if(hidden)(void)hideObject3DProxyCoveredPolygons(*data.meshes[0].topology,std::array<std::uint32_t,4>{0,1,3,4});
    const auto source=firstFace?"v .1 .2 .1\nv .8 .2 .1\nv .4 .8 .1\nf 1 2 3\n":"v 1.2 .2 .1\nv 1.8 .2 .1\nv 1.4 .8 .1\nf 1 2 3\n";
    auto part=parseObject3DObj(source).meshes[0];part.name="Follower";data.meshes.push_back(std::move(part));
    auto binding=bindMeshSurface(data,0,1);(void)transferMeshBindingWeights(data,binding);
    std::vector<SurfaceBinding> bindings{binding};
    if(extra)
    {
        auto temporary=data.meshes[1];temporary.name="Temporary";data.meshes.push_back(std::move(temporary));
        const std::array rows{SurfaceControlPoint{0,{1,1,1},{1,0,0},{0,0,.1}},SurfaceControlPoint{1,{2,2,2},{1,0,0},{0,0,.1}},SurfaceControlPoint{2,{5,5,5},{1,0,0},{0,0,.1}}};
        bindings.push_back(bindMeshControlPoints(data,0,2,rows));fitMeshSurfaceBinding(data,bindings.back());(void)transferMeshBindingWeights(data,bindings.back());
    }
    Object3DTemplateDefinition definition;definition.base=engine.createObject(std::move(data));definition.surfaces=std::move(bindings);
    definition.targets={{"height",0,{{5,{0,.1,0}}}}};return engine.createTemplate(std::move(definition));
}
void visibilityTriangles(const char *fixturePath=nullptr)
{
    Object3DEngine engine;const auto old=frameTemplate(engine,false,false),next=frameTemplate(engine,true,false);
    auto reference=engine.editTemplate(old);reference.get().moveControlPoints(0,std::array{TopologyPointMove{5,{0,0,.02}}});
    reference.get().setTemplateValues(std::array{TemplateValue{"height",1}});
    const auto expected=reference.get().object().get().data().meshes[1].topology->points;
    auto editor=engine.editTemplate(old);editor.get().moveControlPoints(0,std::array{TopologyPointMove{5,{0,0,.02}}});
    const auto before=editor.get().object().get().data().meshes[1].topology->points;
    CHECK(editor.get().replaceTemplateSurfaces(next,{},std::array<std::size_t,1>{0}));
    for(std::size_t i=0;i<before.size();++i)
    {
        const auto p=editor.get().object().get().data().meshes[1].topology->points[i].position;
        CHECK(std::hypot(p.x-before[i].position.x,p.y-before[i].position.y,p.z-before[i].position.z)<1e-10);
    }
    CHECK(editor.get().undo()&&editor.get().redo());
    const auto bytes=exportObject3DProject(editor);writeFixture(fixturePath,bytes);
    Object3DEngine restored;auto project=importObject3DProject(restored.reference(),bytes);
    project.editor.get().setTemplateValues(std::array{TemplateValue{"height",1}});
    for(std::size_t i=0;i<expected.size();++i)
    {
        const auto p=project.editor.get().object().get().data().meshes[1].topology->points[i].position;
        CHECK(std::hypot(p.x-expected[i].position.x,p.y-expected[i].position.y,p.z-expected[i].position.z)<1e-10);
    }
    auto reveal=engine.editTemplate(next);const auto previous=reveal.get().object().get().data().meshes[1].topology->points;
    CHECK(reveal.get().replaceTemplateSurfaces(old,{},std::array<std::size_t,1>{0}));
    for(std::size_t i=0;i<previous.size();++i)
    {
        const auto p=reveal.get().object().get().data().meshes[1].topology->points[i].position;
        CHECK(std::hypot(p.x-previous[i].position.x,p.y-previous[i].position.y,p.z-previous[i].position.z)<1e-10);
    }
    const auto invalid=frameTemplate(engine,false,true),covered=frameTemplate(engine,true,true);
    auto anchored=engine.editTemplate(invalid);const auto revision=anchored.get().revision();
    rejects([&]{(void)anchored.get().replaceTemplateSurfaces(covered,{},std::array<std::size_t,1>{0});});
    CHECK(anchored.get().revision()==revision);
    const auto withExtra=frameTemplate(engine,false,false,true);auto removing=engine.editTemplate(withExtra);
    const auto frame=removing.get().object().get().data().meshes[1].topology->points;
    CHECK(removing.get().removeTemplateSurfaces(next,std::array<std::size_t,1>{2},std::array<std::size_t,1>{0}));
    for(std::size_t i=0;i<frame.size();++i)
    {
        const auto p=removing.get().object().get().data().meshes[1].topology->points[i].position;
        CHECK(std::hypot(p.x-frame[i].position.x,p.y-frame[i].position.y,p.z-frame[i].position.z)<1e-10);
    }
}
Object3DTemplateHandle removalTemplate(Object3DEngine &engine,bool removeFirst,bool chained=false,bool alternateFirst=false)
{
    auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    data.bones={{"root",NoBone,{}},{"child",0,{}}};for(auto &point:data.meshes[0].topology->points)point.weights={{0,1}};
    std::vector<SurfaceBinding> bindings;
    for(const auto *name:{"A","B","C"})
    {
        if(removeFirst&&std::string_view(name)=="A")continue;
        auto part=parseObject3DObj("v 0 0 .1\nv 1 0 .1\nv 0 1 .1\nf 1 2 3\n").meshes[0];
        part.name=alternateFirst&&std::string_view(name)=="A"?"AltA":name;
        Object3DMaterial material;material.name=part.name+"Material";part.material=data.materials.size();data.materials.push_back(material);
        auto layout=part.topology->layout->definition();layout.polygonMaterials.assign(layout.polygons.size(),static_cast<std::uint32_t>(part.material));
        const auto triangles=part.topology->layout->triangles(),faces=part.topology->layout->trianglePolygons();
        part.topology->layout=MeshTopologyLayout::restore(std::move(layout),part.topology->points,
            std::vector<std::uint32_t>(triangles.begin(),triangles.end()),std::vector<std::uint32_t>(faces.begin(),faces.end()));
        data.meshes.push_back(std::move(part));const auto target=data.meshes.size()-1;
        const std::array rows{SurfaceControlPoint{0,{0,0,0},{1,0,0},{0,0,.1}},SurfaceControlPoint{1,{1,1,1},{1,0,0},{0,0,.1}},SurfaceControlPoint{2,{2,2,2},{1,0,0},{0,0,.1}}};
        const auto source=chained&&std::string_view(name)=="B"?1:0;
        bindings.push_back(bindMeshControlPoints(data,source,target,rows));fitMeshSurfaceBinding(data,bindings.back());(void)transferMeshBindingWeights(data,bindings.back());
    }
    Object3DTemplateDefinition definition;definition.base=engine.createObject(std::move(data));definition.surfaces=std::move(bindings);
    definition.targets={{"height",0,{{2,{0,.1,0}}}}};return engine.createTemplate(std::move(definition));
}
void removal(const char *fixturePath=nullptr)
{
    Object3DEngine engine;const auto old=removalTemplate(engine,false),next=removalTemplate(engine,true);
    auto editor=engine.editTemplate(old);editor.get().moveControlPoints(0,std::array{TopologyPointMove{0,{0,0,.02}}});
    editor.get().moveControlPoints(2,std::array{TopologyPointMove{1,{0,0,.03}}});
    const auto layer=editor.get().layers().back().id;
    editor.get().setLayerPointMasks(layer,std::array{MeshPointMask{2,{0,1,0}}});
    editor.get().setPointSelection("Body and B",std::array{MeshPointMask{0,{1,0,0}},MeshPointMask{2,{0,1,0}}});
    const auto body=editor.get().object().get().data().meshes[0].topology->points[0].position;
    const auto part=editor.get().object().get().data().meshes[2].topology->points[1].position;
    const auto revision=editor.get().revision();
    CHECK(editor.get().removeTemplateSurfaces(next,std::array<std::size_t,1>{1}));
    CHECK(editor.get().revision()==revision+1&&editor.get().object().get().data().meshes.size()==3);
    CHECK(editor.get().object().get().data().meshes[1].name=="B");
    CHECK(near(editor.get().object().get().data().meshes[0].topology->points[0].position.z,body.z));
    CHECK(near(editor.get().object().get().data().meshes[1].topology->points[1].position.z,part.z));
    CHECK(editor.get().layerPointMasks(layer)[0].mesh==1&&editor.get().layerPointMasks(layer)[0].weights[1]==1);
    CHECK(editor.get().pointSelection("Body and B")[1].mesh==1&&editor.get().pointSelection("Body and B")[1].weights[1]==1);
    const auto bytes=exportObject3DProject(editor);CHECK(bytes[8]==33);writeFixture(fixturePath,bytes);
    std::vector<std::uint8_t> marker{4,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,1,2,0,0,0,0,0,0,0};
    auto broken=bytes;const auto found=std::search(broken.begin()+24,broken.end(),marker.begin(),marker.end());
    CHECK(found!=broken.end()&&std::search(found+1,broken.end(),marker.begin(),marker.end())==broken.end());
    *(found+9)=250;repairChecksum(broken);Object3DEngine rejected;
    rejects([&]{(void)importObject3DProject(rejected.reference(),broken);});CHECK(rejected.objectCount()==0&&rejected.templateCount()==0);
    Object3DEngine destination;auto project=importObject3DProject(destination.reference(),bytes);
    CHECK(project.editor.get().undo()&&project.editor.get().object().get().data().meshes[2].name=="B");
    CHECK(project.editor.get().pointSelection("Body and B")[1].mesh==2&&project.editor.get().layerPointMasks(layer)[0].mesh==2);
    CHECK(project.editor.get().redo()&&project.editor.get().pointSelection("Body and B")[1].mesh==1);
    project.editor.get().setTemplateValues(std::array{TemplateValue{"height",1}});
    CHECK(near(project.editor.get().object().get().data().meshes[1].topology->points[2].position.y,1.1));
    auto protectedEditor=engine.editTemplate(old);protectedEditor.get().moveControlPoints(1,std::array{TopologyPointMove{0,{0,0,.04}}});
    protectedEditor.get().setPointSelection("A",std::array{MeshPointMask{1,{1,0,0}}});
    const auto protectedRevision=protectedEditor.get().revision();
    rejects([&]{(void)protectedEditor.get().removeTemplateSurfaces(next,std::array<std::size_t,1>{1});});
    CHECK(protectedEditor.get().revision()==protectedRevision);
    CHECK(protectedEditor.get().removeTemplateSurfaces(next,std::array<std::size_t,1>{1},{},TemplateSurfaceRemovalOptions{true}));
    CHECK(protectedEditor.get().pointSelection("A").empty());
    Object3DEngine discarded;auto discardedProject=importObject3DProject(discarded.reference(),exportObject3DProject(protectedEditor));
    CHECK(discardedProject.editor.get().pointSelection("A").empty());
    CHECK(discardedProject.editor.get().undo()&&discardedProject.editor.get().pointSelection("A")[0].mesh==1);
    CHECK(protectedEditor.get().undo()&&protectedEditor.get().pointSelection("A")[0].mesh==1);
    auto shortHistory=engine.editTemplate(old,{},1);shortHistory.get().setPointSelection("B",std::array{MeshPointMask{2,{1,0,0}}});
    CHECK(shortHistory.get().removeTemplateSurfaces(next,std::array<std::size_t,1>{1}));
    shortHistory.get().setTemplateValues(std::array{TemplateValue{"height",.5}});
    Object3DEngine pruned;auto prunedProject=importObject3DProject(pruned.reference(),exportObject3DProject(shortHistory));
    CHECK(prunedProject.editor.get().pointSelection("B")[0].mesh==1&&prunedProject.editor.get().undo());
    auto invalid=engine.editTemplate(old);const auto invalidRevision=invalid.get().revision();
    const auto objects=engine.objectCount(),programs=engine.templateCount();
    rejects([&]{(void)invalid.get().removeTemplateSurfaces(next,std::array<std::size_t,2>{1,1});});
    rejects([&]{(void)invalid.get().removeTemplateSurfaces(next,std::array<std::size_t,1>{0});});
    Object3DEngine other;const auto foreign=removalTemplate(other,true);
    rejects([&]{(void)invalid.get().removeTemplateSurfaces(foreign,std::array<std::size_t,1>{1});});
    CHECK(invalid.get().revision()==invalidRevision&&engine.objectCount()==objects&&engine.templateCount()==programs);
    const auto source=removalTemplate(engine,false,true);auto dependent=engine.editTemplate(source);
    const auto beforeRevision=dependent.get().revision();rejects([&]{(void)dependent.get().removeTemplateSurfaces(next,std::array<std::size_t,1>{1});});
    CHECK(dependent.get().revision()==beforeRevision&&dependent.get().object().get().data().meshes.size()==4);
}
void addition(const char *fixturePath=nullptr)
{
    Object3DEngine engine;const auto old=removalTemplate(engine,true),next=removalTemplate(engine,false);
    const auto variant=removalTemplate(engine,false,false,true);
    auto editor=engine.editTemplate(old);editor.get().moveControlPoints(0,std::array{TopologyPointMove{0,{0,0,.02}}});
    editor.get().setControlWeights(0,std::array{TopologyPointWeights{0,{{1,1}}}});
    editor.get().setSkinningProfile(0,SkinningProfile{SkinningMethod::DualQuaternion,{}});
    editor.get().moveControlPoints(1,std::array{TopologyPointMove{1,{0,0,.03}}});
    const auto layer=editor.get().layers().back().id;editor.get().setLayerPointMasks(layer,std::array{MeshPointMask{1,{0,1,0}}});
    editor.get().setPointSelection("Body and B",std::array{MeshPointMask{0,{1,0,0}},MeshPointMask{1,{0,1,0}}});
    const auto before=editor.get().object().get().data().meshes[1].topology->points[1].position;
    const auto revision=editor.get().revision();
    CHECK(editor.get().addTemplateSurfaces(next,std::array<std::size_t,1>{1}));
    const auto &data=editor.get().object().get().data();
    CHECK(editor.get().revision()==revision+1&&data.meshes.size()==4&&data.meshes[1].name=="A"&&data.meshes[2].name=="B");
    CHECK(near(data.meshes[2].topology->points[1].position.z,before.z));
    CHECK(data.meshes[1].topology->points[0].weights.size()==1&&data.meshes[1].topology->points[0].weights[0].bone==1);
    CHECK(data.meshes[1].topology->skinning.method==SkinningMethod::DualQuaternion);
    CHECK(editor.get().pointSelection("Body and B")[1].mesh==2&&editor.get().layerPointMasks(layer)[0].mesh==2);
    CHECK(editor.get().replaceTemplateSurfaces(variant,std::array<std::size_t,1>{1}));
    CHECK(editor.get().object().get().data().meshes[1].name=="AltA"&&editor.get().undo());
    const auto bytes=exportObject3DProject(editor);CHECK(bytes[8]==34);writeFixture(fixturePath,bytes);
    const std::vector<std::uint8_t> marker{3,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1,2,0,0,0,0,0,0,0,1,3,0,0,0,0,0,0,0};
    auto damaged=bytes;const auto found=std::search(damaged.begin()+24,damaged.end(),marker.begin(),marker.end());
    CHECK(found!=damaged.end()&&std::search(found+1,damaged.end(),marker.begin(),marker.end())==damaged.end());
    *(found+27)=2;repairChecksum(damaged);Object3DEngine rejected;
    rejects([&]{(void)importObject3DProject(rejected.reference(),damaged);});CHECK(rejected.objectCount()==0&&rejected.templateCount()==0);
    Object3DEngine destination;auto project=importObject3DProject(destination.reference(),bytes);
    CHECK(project.editor.get().undo()&&project.editor.get().pointSelection("Body and B")[1].mesh==1);
    CHECK(project.editor.get().redo()&&project.editor.get().pointSelection("Body and B")[1].mesh==2);
    project.editor.get().setTemplateValues(std::array{TemplateValue{"height",1}});
    CHECK(near(project.editor.get().object().get().data().meshes[1].topology->points[2].position.y,1.1));
    auto invalid=engine.editTemplate(old);const auto invalidRevision=invalid.get().revision();
    const auto objects=engine.objectCount(),programs=engine.templateCount();
    rejects([&]{(void)invalid.get().addTemplateSurfaces(next,std::array<std::size_t,2>{1,1});});
    Object3DEngine other;const auto foreign=removalTemplate(other,false);
    rejects([&]{(void)invalid.get().addTemplateSurfaces(foreign,std::array<std::size_t,1>{1});});
    CHECK(invalid.get().revision()==invalidRevision&&engine.objectCount()==objects&&engine.templateCount()==programs);
}
Object3DTemplateHandle additionGraphTemplate(Object3DEngine &engine,bool parts)
{
    auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    data.bones={{"root",NoBone,{}},{"child",0,{}}};for(auto &point:data.meshes[0].topology->points)point.weights={{0,1}};
    std::vector<SurfaceBinding> bindings;
    if(parts)
    {
        auto first=data.meshes[0],second=data.meshes[0];first.name="Outer";second.name="Inner";
        data.meshes.push_back(std::move(first));data.meshes.push_back(std::move(second));
        data.meshes[2].topology->morphTargets.push_back({"Lift",{{0,{0,0,.1}}}});
        const std::array rows{SurfaceControlPoint{0,{0,0,0},{1,0,0},{0,0,.1}},SurfaceControlPoint{1,{1,1,1},{1,0,0},{0,0,.1}},SurfaceControlPoint{2,{2,2,2},{1,0,0},{0,0,.1}}};
        const auto inner=bindMeshControlPoints(data,0,2,rows);fitMeshSurfaceBinding(data,inner);(void)transferMeshBindingWeights(data,inner);
        const auto outer=bindMeshControlPoints(data,2,1,rows);fitMeshSurfaceBinding(data,outer);(void)transferMeshBindingWeights(data,outer);
        bindings={outer,inner}; // The target order is opposite to dependency order.
    }
    Object3DTemplateDefinition definition;definition.base=engine.createObject(std::move(data));definition.surfaces=std::move(bindings);
    return engine.createTemplate(std::move(definition));
}
void additionGraph(const char *fixturePath=nullptr)
{
    Object3DEngine engine;const auto body=additionGraphTemplate(engine,false),candidate=additionGraphTemplate(engine,true);
    auto editor=engine.editTemplate(body);editor.get().setControlWeights(0,std::array{TopologyPointWeights{0,{{1,1}}}});
    editor.get().setSkinningProfile(0,SkinningProfile{SkinningMethod::DualQuaternion,{}});
    CHECK(editor.get().addTemplateSurfaces(candidate,std::array<std::size_t,2>{1,2}));
    for(const auto mesh:{1,2})
    {
        const auto &surface=*editor.get().object().get().data().meshes[mesh].topology;
        CHECK(surface.points[0].weights.size()==1&&surface.points[0].weights[0].bone==1&&surface.skinning.method==SkinningMethod::DualQuaternion);
    }
    CHECK(editor.get().object().get().data().meshes[2].topology->morphTargets[0].name=="Lift");
    CHECK(editor.get().object().get().data().meshes[2].geometry.morphTargets[0].name=="Lift");
    const auto graphBytes=exportObject3DProject(editor);writeFixture(fixturePath,graphBytes);
    Object3DEngine restored;auto project=importObject3DProject(restored.reference(),graphBytes);
    CHECK(project.editor.get().object().get().data().meshes[2].topology->morphTargets[0].name=="Lift");
    auto changed=candidate.get().definition();changed.targets.push_back({"Unsupported",1,{{0,{0,.01,0}}}});
    const auto invalid=engine.createTemplate(std::move(changed));auto clean=engine.editTemplate(body);
    const auto revision=clean.get().revision();const auto objects=engine.objectCount(),programs=engine.templateCount();
    rejects([&]{(void)clean.get().addTemplateSurfaces(invalid,std::array<std::size_t,2>{1,2});});
    CHECK(clean.get().revision()==revision&&engine.objectCount()==objects&&engine.templateCount()==programs);
    auto different=candidate.get().definition();auto differentBase=different.base.get().data();
    differentBase.meshes[0].topology->points[0].position.x+=.01;different.base=engine.createObject(std::move(differentBase));
    const auto shifted=engine.createTemplate(std::move(different));const auto shiftedObjects=engine.objectCount(),shiftedPrograms=engine.templateCount();
    rejects([&]{(void)clean.get().addTemplateSurfaces(shifted,std::array<std::size_t,2>{1,2});});
    CHECK(clean.get().revision()==revision&&engine.objectCount()==shiftedObjects&&engine.templateCount()==shiftedPrograms);
}
Object3DTemplateHandle compositionTemplate(Object3DEngine &engine,std::initializer_list<std::string_view> names)
{
    auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    data.bones={{"root",NoBone,{}},{"child",0,{}}};for(auto &point:data.meshes[0].topology->points)point.weights={{0,1}};
    std::vector<SurfaceBinding> bindings;
    for(const auto name:names)
    {
        auto part=parseObject3DObj("v 0 0 .1\nv 1 0 .1\nv 0 1 .1\nf 1 2 3\n").meshes[0];
        part.name=std::string(name);Object3DMaterial material;material.name=part.name+"Material";
        part.material=data.materials.size();data.materials.push_back(material);
        auto layout=part.topology->layout->definition();layout.polygonMaterials.assign(layout.polygons.size(),static_cast<std::uint32_t>(part.material));
        const auto triangles=part.topology->layout->triangles(),faces=part.topology->layout->trianglePolygons();
        part.topology->layout=MeshTopologyLayout::restore(std::move(layout),part.topology->points,
            std::vector<std::uint32_t>(triangles.begin(),triangles.end()),std::vector<std::uint32_t>(faces.begin(),faces.end()));
        data.meshes.push_back(std::move(part));const auto target=data.meshes.size()-1;
        const std::array rows{SurfaceControlPoint{0,{0,0,0},{1,0,0},{0,0,.1}},SurfaceControlPoint{1,{1,1,1},{1,0,0},{0,0,.1}},SurfaceControlPoint{2,{2,2,2},{1,0,0},{0,0,.1}}};
        bindings.push_back(bindMeshControlPoints(data,0,target,rows));fitMeshSurfaceBinding(data,bindings.back());(void)transferMeshBindingWeights(data,bindings.back());
    }
    Object3DTemplateDefinition definition;definition.base=engine.createObject(std::move(data));definition.surfaces=std::move(bindings);
    definition.targets={{"height",0,{{2,{0,.1,0}}}}};return engine.createTemplate(std::move(definition));
}
void composition(const char *fixturePath=nullptr,bool captureDiscard=false)
{
    Object3DEngine engine;const auto old=compositionTemplate(engine,{"A","B","C"}),next=compositionTemplate(engine,{"B","D","C","E"});
    auto editor=engine.editTemplate(old);editor.get().moveControlPoints(0,std::array{TopologyPointMove{0,{0,0,.02}}});
    editor.get().setControlWeights(0,std::array{TopologyPointWeights{0,{{1,1}}}});
    editor.get().moveControlPoints(2,std::array{TopologyPointMove{1,{0,0,.03}}});
    const auto layer=editor.get().layers().back().id;editor.get().setLayerPointMasks(layer,std::array{MeshPointMask{2,{0,1,0}}});
    editor.get().setPointSelection("Retained",std::array{MeshPointMask{0,{1,0,0}},MeshPointMask{2,{0,1,0}}});
    const auto body=editor.get().object().get().data().meshes[0].topology->points[0].position;
    const auto part=editor.get().object().get().data().meshes[2].topology->points[1].position;
    const auto revision=editor.get().revision();
    CHECK(editor.get().changeTemplateSurfaces(next,std::array<std::size_t,1>{1},std::array<std::size_t,2>{2,4}));
    const auto &data=editor.get().object().get().data();
    CHECK(editor.get().revision()==revision+1&&data.meshes.size()==5&&data.meshes[1].name=="B"&&data.meshes[2].name=="D"&&data.meshes[4].name=="E");
    CHECK(near(data.meshes[0].topology->points[0].position.z,body.z)&&near(data.meshes[1].topology->points[1].position.z,part.z));
    CHECK(data.meshes[2].topology->points[0].weights[0].bone==1&&data.meshes[4].topology->points[0].weights[0].bone==1);
    CHECK(editor.get().pointSelection("Retained")[1].mesh==1&&editor.get().layerPointMasks(layer)[0].mesh==1);
    const auto bytes=exportObject3DProject(editor);CHECK(bytes[8]==35);
    if(!captureDiscard)writeFixture(fixturePath,bytes);
    const std::vector<std::uint8_t> marker{4,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,1,3,0,0,0,0,0,0,0};
    auto damaged=bytes;const auto found=std::search(damaged.begin()+24,damaged.end(),marker.begin(),marker.end());
    CHECK(found!=damaged.end()&&std::search(found+1,damaged.end(),marker.begin(),marker.end())==damaged.end());
    *(found+28)=1;repairChecksum(damaged);Object3DEngine rejected;
    rejects([&]{(void)importObject3DProject(rejected.reference(),damaged);});CHECK(rejected.objectCount()==0&&rejected.templateCount()==0);
    auto oldVersion=bytes;oldVersion[8]=34;Object3DEngine incompatible;
    rejects([&]{(void)importObject3DProject(incompatible.reference(),oldVersion);});
    Object3DEngine destination;auto project=importObject3DProject(destination.reference(),bytes);
    CHECK(project.editor.get().undo()&&project.editor.get().pointSelection("Retained")[1].mesh==2);
    CHECK(project.editor.get().redo()&&project.editor.get().pointSelection("Retained")[1].mesh==1);
    project.editor.get().setTemplateValues(std::array{TemplateValue{"height",1}});
    CHECK(near(project.editor.get().object().get().data().meshes[2].topology->points[2].position.y,1.1));
    auto protectedEditor=engine.editTemplate(old);protectedEditor.get().moveControlPoints(1,std::array{TopologyPointMove{0,{0,0,.04}}});
    protectedEditor.get().setPointSelection("Removed",std::array{MeshPointMask{1,{1,0,0}}});
    const auto before=protectedEditor.get().revision();const auto objects=engine.objectCount(),programs=engine.templateCount();
    rejects([&]{(void)protectedEditor.get().changeTemplateSurfaces(next,std::array<std::size_t,1>{1},std::array<std::size_t,2>{2,4});});
    CHECK(protectedEditor.get().revision()==before&&engine.objectCount()==objects&&engine.templateCount()==programs);
    CHECK(protectedEditor.get().changeTemplateSurfaces(next,std::array<std::size_t,1>{1},std::array<std::size_t,2>{2,4},{},TemplateSurfaceRemovalOptions{true}));
    CHECK(protectedEditor.get().pointSelection("Removed").empty());
    const auto discardedBytes=exportObject3DProject(protectedEditor);
    if(captureDiscard)writeFixture(fixturePath,discardedBytes);
    Object3DEngine discarded;auto restored=importObject3DProject(discarded.reference(),discardedBytes);
    CHECK(restored.editor.get().pointSelection("Removed").empty()&&restored.editor.get().undo());
    CHECK(restored.editor.get().pointSelection("Removed")[0].mesh==1);
    auto invalid=engine.editTemplate(old);const auto invalidRevision=invalid.get().revision();
    rejects([&]{(void)invalid.get().changeTemplateSurfaces(next,std::array<std::size_t,2>{1,1},std::array<std::size_t,2>{2,4});});
    rejects([&]{(void)invalid.get().changeTemplateSurfaces(next,std::array<std::size_t,1>{1},std::array<std::size_t,2>{2,2});});
    const auto reordered=compositionTemplate(engine,{"C","D","B","E"});
    rejects([&]{(void)invalid.get().changeTemplateSurfaces(reordered,std::array<std::size_t,1>{1},std::array<std::size_t,2>{2,4});});
    Object3DEngine other;const auto foreign=compositionTemplate(other,{"B","D","C","E"});
    rejects([&]{(void)invalid.get().changeTemplateSurfaces(foreign,std::array<std::size_t,1>{1},std::array<std::size_t,2>{2,4});});
    CHECK(invalid.get().revision()==invalidRevision);
    const auto equal=compositionTemplate(engine,{"B","D","C"});auto equalEditor=engine.editTemplate(old);
    equalEditor.get().setPointSelection("B",std::array{MeshPointMask{2,{0,1,0}}});
    CHECK(equalEditor.get().changeTemplateSurfaces(equal,std::array<std::size_t,1>{1},std::array<std::size_t,1>{2}));
    const auto equalBytes=exportObject3DProject(equalEditor);CHECK(equalBytes[8]==35);
    Object3DEngine equalDestination;auto equalProject=importObject3DProject(equalDestination.reference(),equalBytes);
    CHECK(equalProject.editor.get().pointSelection("B")[0].mesh==1&&equalProject.editor.get().undo());
    CHECK(equalProject.editor.get().pointSelection("B")[0].mesh==2);
    const auto chained=removalTemplate(engine,false,true);auto dependent=engine.editTemplate(chained);
    const auto chainedRevision=dependent.get().revision();
    rejects([&]{(void)dependent.get().changeTemplateSurfaces(next,std::array<std::size_t,1>{1},std::array<std::size_t,2>{2,4});});
    CHECK(dependent.get().revision()==chainedRevision);
}
int main(int argc,char **argv)
{
    try{if(argc!=2&&argc!=3&&argc!=5)return 2;const std::string_view mode=argv[1];if(argc==5){if(mode!="replacement_resources")return 2;replacementResources(argv[2],argv[3],argv[4]);return 0;}if(argc==3&&mode!="swap"&&mode!="visibility_triangles"&&mode!="composition"&&mode!="composition_discard"&&mode!="addition"&&mode!="addition_graph"&&mode!="removal")return 2;if(mode=="swap")swapping(argc==3?argv[2]:nullptr);else if(mode=="safeguards")safeguards();else if(mode=="sharing")sharing();else if(mode=="grounding")grounding();else if(mode=="visibility_triangles")visibilityTriangles(argc==3?argv[2]:nullptr);else if(mode=="removal")removal(argc==3?argv[2]:nullptr);else if(mode=="addition")addition(argc==3?argv[2]:nullptr);else if(mode=="addition_graph")additionGraph(argc==3?argv[2]:nullptr);else if(mode=="composition"||mode=="composition_discard")composition(argc==3?argv[2]:nullptr,mode=="composition_discard");else return 2;}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
