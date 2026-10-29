#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/Object3DProxy.h>
#include <iiActorObject/Object3DNumberParsing.h>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <locale>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
void near(Vector3 a,Vector3 b){CHECK(std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)<1e-10);}
Object3DData data()
{
    auto d=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 2 0 1\nf 1 2 3 4\n");auto follower=d.meshes[0];follower.name="Follower";d.meshes.push_back(follower);return d;
}
void weighted()
{
    auto d=data();const std::array rows{SurfaceControlPoint{0,{4,1,2},{1.1,-.1,0},{0,0,.05}}};auto binding=bindMeshControlPoints(d,0,1,rows);const auto positions=evaluateMeshSurfaceBinding(d,binding);near(positions[0],{2.1,0,1.15});d.meshes[1].topology->points[0].position=positions[0];const auto reference=d;
    d.meshes[0].topology->points[4].position.x+=.2;d.meshes[1].topology->points[0].position.y+=.03;applyMeshSurfaceBindings(d,reference,std::span(&binding,1));near(d.meshes[1].topology->points[0].position,{2.32,.03,1.15});unapplyMeshSurfaceBindings(d,reference,std::span(&binding,1));near(d.meshes[1].topology->points[0].position,{2.1,.03,1.15});
}
void scale()
{
    auto d=data();std::array<std::optional<SurfaceBindingAxisScale>,3> scales{};scales[0]=SurfaceBindingAxisScale{0,1,1};const std::array rows{SurfaceControlPoint{0,{4,4,4},{1,0,0},{.1,0,0}}};auto binding=bindMeshControlPoints(d,0,1,rows,scales);near(evaluateMeshSurfaceBinding(d,binding)[0],{2.1,0,1});d.meshes[1].topology->points[0].position={2.1,0,1};const auto reference=d;
    d.meshes[0].topology->points[1].position.x=2;applyMeshSurfaceBindings(d,reference,std::span(&binding,1));near(d.meshes[1].topology->points[0].position,{2.2,0,1});
    auto collapsed=reference;collapsed.meshes[0].topology->points[1].position.x=0;rejects([&]{applyMeshSurfaceBindings(collapsed,reference,std::span(&binding,1));});near(collapsed.meshes[1].topology->points[0].position,{2.1,0,1});
}
void basis()
{
    auto original=data();std::array<std::optional<SurfaceBindingAxisScale>,3> scales{};
    scales[0]=SurfaceBindingAxisScale{0,1,1};scales[1]=SurfaceBindingAxisScale{0,3,1};
    const std::array rows{SurfaceControlPoint{0,{4,4,4},{1,0,0},{.1,.2,.3}}};
    auto binding=bindMeshControlPoints(original,0,1,rows,scales);
    const auto initial=evaluateMeshSurfaceBinding(original,binding)[0];
    const auto linear=[](Vector3 p){return Vector3{2*p.z,3*p.y,-4*p.x};};
    const auto place=[&](Vector3 p){const auto v=linear(p);return Vector3{v.x+4,v.y-2,v.z+3};};
    auto transformed=original;
    for(auto &mesh:transformed.meshes)for(auto &point:mesh.topology->points)point.position=place(point.position);
    const SurfaceBindingBasis offsetBasis{linear({1,0,0}),linear({0,1,0}),linear({0,0,1})};
    const std::array bases{std::optional<SurfaceBindingBasis>{offsetBasis}};
    rejects([&]{(void)bindMeshControlPoints(transformed,0,1,rows,scales);});
    const auto authored=bindMeshControlPointsWithBasis(transformed,0,1,rows,scales,offsetBasis);
    near(evaluateMeshSurfaceBindingWithBasis(transformed,authored,offsetBasis)[0],place(initial));
    near(evaluateMeshSurfaceBindingWithBasis(transformed,binding,offsetBasis)[0],place(initial));
    transformed.meshes[1].topology->points[0].position=place(initial);const auto reference=transformed;
    auto changedOriginal=original;changedOriginal.meshes[0].topology->points[1].position.x=2;
    changedOriginal.meshes[0].topology->points[3].position.y=1.5;
    transformed.meshes[0].topology->points[1].position=place({2,0,0});
    transformed.meshes[0].topology->points[3].position=place({0,1.5,0});
    applyMeshSurfaceBindingsWithBases(transformed,reference,std::span(&binding,1),bases);
    const auto &oldBinding=binding;
    near(transformed.meshes[1].topology->points[0].position,
         place(evaluateMeshSurfaceBinding(changedOriginal,oldBinding)[0]));
    unapplyMeshSurfaceBindingsWithBases(transformed,reference,std::span(&binding,1),bases);
    near(transformed.meshes[1].topology->points[0].position,place(initial));
    auto fitted=reference;fitMeshSurfaceBindingWithBasis(fitted,binding,offsetBasis);
    near(fitted.meshes[1].topology->points[0].position,place(initial));
    auto skinned=reference;skinned.bones={{"root",NoBone,{}}};
    for(auto &point:skinned.meshes[0].topology->points)point.weights={{0,1}};
    CHECK(transferMeshBindingWeightsWithBasis(skinned,binding,offsetBasis).transferred==1);
    CHECK(skinned.meshes[1].topology->points[0].weights.size()==1&&skinned.meshes[1].topology->points[0].weights[0].bone==0);
    auto invalid=SurfaceBindingBasis{Vector3{1,0,0},Vector3{1,0,0},Vector3{0,0,1}};
    const auto before=transformed.meshes[1].topology->points[0].position;
    const std::array badBases{std::optional<SurfaceBindingBasis>{invalid}};
    rejects([&]{applyMeshSurfaceBindingsWithBases(transformed,reference,std::span(&binding,1),badBases);});
    near(transformed.meshes[1].topology->points[0].position,before);
    invalid=offsetBasis;invalid[0].x=std::numeric_limits<double>::infinity();
    rejects([&]{(void)evaluateMeshSurfaceBindingWithBasis(reference,binding,invalid);});
    invalid=offsetBasis;invalid[2]={0,0,1e-14};
    rejects([&]{(void)evaluateMeshSurfaceBindingWithBasis(reference,binding,invalid);});
    const auto geometric=bindMeshSurface(reference,0,1);
    rejects([&]{(void)evaluateMeshSurfaceBindingWithBasis(reference,geometric,offsetBasis);});
    const std::array tooMany{std::optional<SurfaceBindingBasis>{},std::optional<SurfaceBindingBasis>{}};
    rejects([&]{validateMeshSurfaceBindingsWithBases(reference,std::span(&binding,1),tooMany);});
}
void basisProjectFixture(const std::filesystem::path &path)
{
    Object3DEngine engine;auto d=data();std::array<std::optional<SurfaceBindingAxisScale>,3> scales{};
    scales[0]=SurfaceBindingAxisScale{0,1,1};
    const std::array rows{SurfaceControlPoint{0,{0,0,0},{1,0,0},{.1,0,0}}};
    auto binding=bindMeshControlPoints(d,0,1,rows,scales);
    const SurfaceBindingBasis basis{Vector3{2,0,0},Vector3{0,3,0},Vector3{0,0,4}};
    d.meshes[1].topology->points[0].position=evaluateMeshSurfaceBindingWithBasis(d,binding,basis)[0];
    Object3DTemplateDefinition definition;definition.base=engine.createObject(std::move(d));
    definition.targets={{"stretch",0,{{1,{.2,0,0}}}}};definition.surfaces={binding};definition.surfaceOffsetBases={basis};
    auto editor=engine.editTemplate(engine.createTemplate(std::move(definition)));
    editor.get().setTemplateValues(std::array{TemplateValue{"stretch",1}});
    const auto bytes=exportObject3DProject(editor);CHECK(bytes.size()>24&&bytes[8]==25);
    std::filesystem::create_directories(path.parent_path());std::ofstream output(path,std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));CHECK(output.good());
}
void basisProjectRead(const std::filesystem::path &path,bool valid)
{
    std::ifstream input(path,std::ios::binary);CHECK(input.good());
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    Object3DEngine engine;const auto sentinel=engine.createObject(data());const auto count=engine.objectCount();
    bool loaded=false;
    try{auto project=importObject3DProject(engine.reference(),bytes);loaded=project.editor.valid();}
    catch(const std::exception&){if(valid)throw;}
    CHECK(loaded==valid&&sentinel.valid());if(!valid)CHECK(engine.objectCount()==count);
}
void graph()
{
    auto reference=data();auto coat=reference.meshes[1];coat.name="Coat";reference.meshes.push_back(coat);std::vector<SurfaceControlPoint> rows;for(std::uint32_t i=0;i<4;++i)rows.push_back({i,{i,i,i},{1,0,0},{0,0,.05}});auto pointBinding=bindMeshControlPoints(reference,0,1,rows);
    const auto p=evaluateMeshSurfaceBinding(reference,pointBinding);for(std::size_t i=0;i<p.size();++i)reference.meshes[1].topology->points[i].position=p[i];for(auto &v:reference.meshes[2].topology->points)v.position.z+=.1;const auto surface=bindMeshSurface(reference,1,2);const std::array bindings{pointBinding,surface};auto current=reference;current.meshes[0].topology->points[2].position.z+=.02;current.meshes[2].topology->points[2].position.x+=.01;
    applyMeshSurfaceBindings(current,reference,bindings);const auto final=current;unapplyMeshSurfaceBindings(current,reference,bindings);applyMeshSurfaceBindings(current,reference,bindings);for(std::size_t m=0;m<3;++m)for(std::size_t i=0;i<5;++i)near(current.meshes[m].topology->points[i].position,final.meshes[m].topology->points[i].position);
}
void codec()
{
    Object3DEngine engine;auto d=data();const std::array rows{SurfaceControlPoint{0,{4,1,2},{1.1,-.1,0},{0,0,.05}}};auto binding=bindMeshControlPoints(d,0,1,rows);d.meshes[1].topology->points[0].position=evaluateMeshSurfaceBinding(d,binding)[0];Object3DTemplateDefinition program;program.base=engine.createObject(d);program.targets={{"shape",0,{{4,{.2,0,0}}}}};program.surfaces={binding};auto e=engine.editTemplate(engine.createTemplate(program));e.get().setTemplateValues(std::array{TemplateValue{"shape",1}});near(e.get().object().get().data().meshes[1].topology->points[0].position,{2.32,0,1.15});
    Object3DEngine restored;auto project=importObject3DProject(restored.reference(),exportObject3DProject(e));near(project.editor.get().object().get().data().meshes[1].topology->points[0].position,{2.32,0,1.15});CHECK(project.editor.get().undo()&&project.editor.get().redo());
}
void parser()
{
    constexpr std::string_view text="# owned descriptor\nname Example\nbasemesh hm08\nobj_file mesh.obj\nx_scale 0 1 2\nverts 0\n4\n4 1 2 1.1 -.1 0 .1 0 0\n";
    auto parsed=parseObject3DProxy(text,5,2,.1);CHECK(parsed.name=="Example"&&parsed.points.size()==2&&parsed.points[0].vertices[0]==4&&parsed.points[1].weights[1]==-.1);CHECK(std::abs(parsed.offsetScales[0]->referenceSpan-.2)<1e-12);near(parsed.points[1].offset,{.01,0,0});CHECK(parseObject3DProxy(text,5,0).points.size()==2);
    rejects([&]{(void)parseObject3DProxy(text,5,1);});rejects([&]{(void)parseObject3DProxy(text,4,2);});rejects([&]{(void)parseObject3DProxy(text,5,3);});rejects([&]{(void)parseObject3DProxy(text,5,2,1,8);});
    constexpr std::string_view clothing="name Coat\nbasemesh hm08\nobj_file coat.obj\nverts 0\nmaterial coat.mhmat\n0\n1\n2\ndelete_verts\n1 - 3 4\n0 1\n";
    auto coat=parseObject3DProxy(clothing,5,3);
    CHECK(coat.materialFile=="coat.mhmat"&&coat.points.size()==3&&
          coat.hiddenSourcePoints==std::vector<std::uint32_t>({0,1,2,3,4}));
    for(const auto suffix:{"- 3\n","3 -\n","4 - 2\n","0 - 5\n","1 - 2 - 3\n"})
    {
        const std::string invalid="basemesh hm08\nobj_file coat.obj\nverts 0\n0\ndelete_verts\n"+std::string(suffix);
        rejects([&]{(void)parseObject3DProxy(invalid,5,1);});
    }
    rejects([&]{(void)parseObject3DProxy("basemesh hm08\nobj_file coat.obj\nverts 0\n0\ndelete_verts\n0 - 2000000\n",2000001,1);});
}
void portableNumbers()
{
    using iiActorObject::object3d_detail::readClassicReal;
    struct Comma final : std::numpunct<char>{char do_decimal_point() const override{return ',';}};
    const auto saved=std::locale();std::locale::global(std::locale(saved,new Comma));
    double value=0;
    CHECK(readClassicReal("-.5e-1",value)&&std::abs(value+.05)<1e-15);
    CHECK(readClassicReal("1.25",value)&&value==1.25);
    CHECK(readClassicReal("0e-999",value)&&value==0);
    CHECK(readClassicReal("5e-324",value)&&value==std::numeric_limits<double>::denorm_min());
    for(const std::string_view sample:{"0", "-0", ".123", "-1.25e+4", "1.7976931348623157e308", "2.2250738585072014e-308", "-5e-324"})
    {
        double native=0,classic=0;CHECK(iiActorObject::object3d_detail::readNumber(sample,native));
        CHECK(readClassicReal(sample,classic)&&native==classic&&std::signbit(native)==std::signbit(classic));
    }
    for(const std::string_view invalid:{"", "+1", ".", "nan", "inf", "1,5", "1e", "1e--2", "1e400", "1e-999", "0x1", " 1", "1x"})
        CHECK(!readClassicReal(invalid,value));
    std::locale::global(saved);
}
void coveredPolygons()
{
    auto object=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 2 0 0\nv 0 1 0\nv 1 1 .2\nv 2 1 0\nf 1 2 5 4\nf 2 3 6 5\n");
    auto &topology=*object.meshes[0].topology;const auto original=topology.layout;
    const auto layoutsForFace=[](const auto &layout,std::uint32_t face)
    {
        std::vector<std::uint32_t> result;const auto triangles=layout.triangles(),polygons=layout.trianglePolygons();
        for(std::size_t i=0;i<polygons.size();++i)if(polygons[i]==face)
            for(unsigned k=0;k<3;++k)result.push_back(layout.definition().renderToLayout[triangles[3*i+k]]);
        return result;
    };
    const auto expected=layoutsForFace(*original,1);
    CHECK(hideObject3DProxyCoveredPolygons(topology,std::array<std::uint32_t,3>{0,1,3})==0);
    CHECK(topology.layout==original);
    CHECK(hideObject3DProxyCoveredPolygons(topology,std::array<std::uint32_t,4>{0,1,3,4})==1);
    CHECK(topology.layout->definition().hiddenPolygons==std::vector<std::uint32_t>({0}));
    CHECK(topology.layout->definition().polygons==original->definition().polygons&&topology.points.size()==6);
    CHECK(layoutsForFace(*topology.layout,1)==expected);
    const auto masked=topology.layout;
    rejects([&]{(void)hideObject3DProxyCoveredPolygons(topology,std::array<std::uint32_t,1>{6});});
    CHECK(topology.layout==masked);
    rejects([&]{(void)hideObject3DProxyCoveredPolygons(topology,std::array<std::uint32_t,4>{1,2,4,5});});
    CHECK(topology.layout==masked);
    auto corrupt=topology;corrupt.points.pop_back();
    rejects([&]{(void)hideObject3DProxyCoveredPolygons(corrupt,std::array<std::uint32_t,1>{0});});
    CHECK(corrupt.layout==masked);
}
void skin()
{
    auto d=data();d.bones={{"root",NoBone,{}},{"eye",0,{}}};for(auto &p:d.meshes[0].topology->points)p.weights={{0,1}};d.meshes[0].topology->points[4].weights={{1,1}};const std::array rows{SurfaceControlPoint{0,{4,1,2},{1.1,-.1,0},{0,0,.05}}};auto binding=bindMeshControlPoints(d,0,1,rows);fitMeshSurfaceBinding(d,binding);CHECK(transferMeshBindingWeights(d,binding).transferred==1);CHECK(d.meshes[1].topology->points[0].weights.size()==1&&d.meshes[1].topology->points[0].weights[0].bone==1);
    for(auto &p:d.meshes[0].topology->points)p.weights.clear();const auto before=d.meshes[1].topology->points[0].weights;rejects([&]{(void)transferMeshBindingWeights(d,binding);});CHECK(d.meshes[1].topology->points[0].weights[0].bone==before[0].bone);CHECK(transferMeshBindingWeights(d,binding,1,0).transferred==1&&d.meshes[1].topology->points[0].weights[0].bone==0);
    const std::array envelopes{BoneEnvelope{1,{0,0,0},{0,0,1},1}};
    AutoBindOptions options;options.maxInfluences=1;
    SkinBinder::bindMesh(d,1,envelopes,options);
    for(const auto &point:d.meshes[0].topology->points)CHECK(point.weights.empty());
    for(const auto &point:d.meshes[1].topology->points)CHECK(point.weights.size()==1&&point.weights[0].bone==1);
}
void skinningProfiles()
{
    auto d=data();d.bones={{"root",NoBone,{}},{"eye",0,{}}};
    for(auto &point:d.meshes[0].topology->points)point.weights={{0,1}};
    d.meshes[0].topology->points[4].weights={{1,1}};
    d.meshes[0].topology->skinning={SkinningMethod::Blended,{0,.4,.6,.8,1}};
    const SkinningProfile initial{SkinningMethod::Blended,{.1,.2,.3,.4,.5}};
    d.meshes[1].topology->skinning=initial;
    const std::array rows{SurfaceControlPoint{0,{0,1,4},{.25,.75,0},{}}};
    const auto binding=bindMeshControlPoints(d,0,1,rows);
    auto legacy=d;(void)transferMeshBindingWeights(legacy,binding);
    CHECK(legacy.meshes[1].topology->skinning==initial);
    auto copied=d;(void)transferMeshBindingWeights(copied,binding,0,NoBone,SkinningProfileTransfer::Source);
    const auto &profile=copied.meshes[1].topology->skinning;
    CHECK(profile.method==SkinningMethod::Blended&&std::abs(profile.blendWeights[0]-.3)<1e-12);
    for(std::size_t i=1;i<5;++i)CHECK(profile.blendWeights[i]==initial.blendWeights[i]);
    auto unequal=d;unequal.meshes[0].topology->points[1].weights={{0,3}};
    (void)transferMeshBindingWeights(unequal,binding,0,NoBone,SkinningProfileTransfer::Source);
    CHECK(std::abs(unequal.meshes[1].topology->skinning.blendWeights[0]-.36)<1e-12);
    const auto &layout=copied.meshes[1].topology->layout->definition();
    for(std::size_t i=0;i<layout.renderToLayout.size();++i)
        CHECK(copied.meshes[1].geometry.skinning.blendWeights[i]==profile.blendWeights[layout.layouts[layout.renderToLayout[i]].position]);
    auto hidden=d;const std::array signedRows{SurfaceControlPoint{0,{4,1,2},{1.1,-.1,0},{}}};
    const auto signedBinding=bindMeshControlPoints(hidden,0,1,signedRows);
    (void)transferMeshBindingWeights(hidden,signedBinding,0,NoBone,SkinningProfileTransfer::Source);
    CHECK(hidden.meshes[1].topology->skinning.blendWeights[0]==1);
    auto linear=d;linear.meshes[0].topology->skinning={};linear.meshes[1].topology->skinning={SkinningMethod::DualQuaternion,{}};
    auto unchanged=linear;(void)transferMeshBindingWeights(unchanged,binding,0,NoBone,SkinningProfileTransfer::SourceIfNonlinear);
    CHECK(unchanged.meshes[1].topology->skinning.method==SkinningMethod::DualQuaternion);
    (void)transferMeshBindingWeights(linear,binding,0,NoBone,SkinningProfileTransfer::Source);
    const auto &partial=linear.meshes[1].topology->skinning;
    CHECK(partial.method==SkinningMethod::Blended&&partial.blendWeights==std::vector<double>({0,1,1,1,1}));
    auto failed=d;rejects([&]{(void)transferMeshBindingWeights(failed,binding,0,NoBone,static_cast<SkinningProfileTransfer>(255));});
    CHECK(failed.meshes[1].topology->skinning==initial);
    for(auto &point:failed.meshes[0].topology->points)point.weights.clear();
    (void)transferMeshBindingWeights(failed,binding,0,0,SkinningProfileTransfer::Source);
    CHECK(failed.meshes[1].topology->skinning==initial);
    auto surface=d;const auto geometric=bindMeshSurface(surface,0,1);
    (void)transferMeshBindingWeights(surface,geometric,0,NoBone,SkinningProfileTransfer::Source);
    const auto &result=surface.meshes[1].topology->skinning;
    CHECK(result.method==SkinningMethod::Blended);
    for(std::size_t i=0;i<4;++i)CHECK(std::abs(result.blendWeights[i]-d.meshes[0].topology->skinning.blendWeights[i])<1e-12);
    CHECK(result.blendWeights[4]==initial.blendWeights[4]);
}
int main(int argc,char **argv)
{
    try{if(argc!=2&&argc!=3)return 2;const std::string_view mode=argv[1];
    if(mode=="basis_project_fixture"&&argc==3)basisProjectFixture(argv[2]);
    else if(mode=="basis_project_valid"&&argc==3)basisProjectRead(argv[2],true);
    else if(mode=="basis_project_reject"&&argc==3)basisProjectRead(argv[2],false);
    else if(argc!=2)return 2;else if(mode=="weighted")weighted();else if(mode=="scale")scale();else if(mode=="basis")basis();else if(mode=="graph")graph();else if(mode=="codec")codec();else if(mode=="parser")parser();else if(mode=="numbers")portableNumbers();else if(mode=="covered")coveredPolygons();else if(mode=="skin"){skin();skinningProfiles();}else return 2;std::cout<<mode<<" passed\n";}
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
