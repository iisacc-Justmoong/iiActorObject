#include <iiActorObject/Features.h>
#include <iiActorObject/MeshScreenSelection.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

using namespace iiActorObject;
#define CHECK(x) do { if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x); } while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}

Object3DData fixture()
{
    return parseObject3DObj("v -.8 -.8 -.2\nv .8 -.8 .5\nv .8 .8 .5\nv -.8 .8 -.2\nv 0 0 0\nv 2 0 0\n"
        "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nvt .2 .2\nvt .8 .8\n"
        "f 1/1 2/2 3/3\nf 1/5 3/6 4/4\n");
}
MeshScreenView view()
{
    MeshScreenView result;result.viewport={10,20,100,100,true};return result;
}
void shapes()
{
    Object3DEngine engine;const auto object=engine.createObject(fixture());
    const auto &mesh=object.get().data().meshes[0];CHECK(mesh.topology&&mesh.geometry.vertices.size()>4);
    const auto rectangle=selectMeshPointsOnScreen(object.get(),view(),MeshScreenRectangle{{15,25},{25,115}});
    CHECK(rectangle.masks.size()==1&&rectangle.masks[0].weights==std::vector<double>({1,0,0,1,0,0}));
    CHECK(rectangle.selectedPoints==2&&rectangle.skippedUnusedPoints==2);
    const auto fromData=selectMeshPointsOnScreen(object.get().data(),view(),MeshScreenRectangle{{15,25},{25,115}});
    CHECK(fromData.masks==rectangle.masks&&fromData.selectedPoints==rectangle.selectedPoints&&
          fromData.skippedUnusedPoints==rectangle.skippedUnusedPoints);
    const auto circle=selectMeshPointsOnScreen(object.get(),view(),MeshScreenCircle{{20,30},4});
    CHECK(circle.masks[0].weights==std::vector<double>({0,0,0,1,0,0}));
    const std::array polygon{MeshScreenPoint{10,20},MeshScreenPoint{30,20},
        MeshScreenPoint{30,120},MeshScreenPoint{10,120}};
    const auto lasso=selectMeshPointsOnScreen(object.get(),view(),MeshScreenPolygon{polygon});
    CHECK(lasso.masks==rectangle.masks&&lasso.polygonEdgeTests>0);
    const std::array concave{MeshScreenPoint{10,20},MeshScreenPoint{60,20},MeshScreenPoint{60,70},
        MeshScreenPoint{110,70},MeshScreenPoint{110,120},MeshScreenPoint{10,120}};
    const auto concaveResult=selectMeshPointsOnScreen(object.get(),view(),MeshScreenPolygon{concave});
    CHECK(concaveResult.masks[0].weights==std::vector<double>({1,1,0,1,0,0}));
    const std::array crossed{MeshScreenPoint{0,0},MeshScreenPoint{120,120},
        MeshScreenPoint{120,0},MeshScreenPoint{0,120}};
    const auto evenOdd=selectMeshPointsOnScreen(object.get(),view(),MeshScreenPolygon{crossed});
    CHECK(evenOdd.masks[0].weights==std::vector<double>({0,0,1,1,0,0}));
    auto inverted=view();inverted.viewport.yDown=false;
    const auto bottom=selectMeshPointsOnScreen(object.get(),inverted,MeshScreenCircle{{20,30},4});
    CHECK(bottom.masks[0].weights==std::vector<double>({1,0,0,0,0,0}));
    const std::array edge{MeshScreenPoint{20,30},MeshScreenPoint{100,30},MeshScreenPoint{60,110}};
    const auto boundary=selectMeshPointsOnScreen(object.get(),view(),MeshScreenPolygon{edge});
    CHECK(boundary.masks[0].weights[2]==1&&boundary.masks[0].weights[3]==1);
}
void perspective()
{
    auto data=parseObject3DObj("v -1 -1 2\nv 1 -1 2\nv 0 1 2\nv 0 0 -1\nv 0 0 .5\n"
                               "f 1 2 3\nf 1 4 5\n");
    Object3DEngine engine;const auto object=engine.createObject(std::move(data));MeshScreenView camera;
    camera.viewport={0,0,100,100,true};camera.depthRange=MeshScreenDepthRange::ZeroToOne;
    camera.clipFromObject={1,0,0,0, 0,1,0,0, 0,0,1,1, 0,0,-1,0};
    const auto result=selectMeshPointsOnScreen(object.get(),camera,MeshScreenRectangle{{0,0},{100,100}});
    CHECK(result.masks[0].weights==std::vector<double>({1,1,1,0,0}));
    CHECK(result.clippedPoints==2&&result.selectedPoints==3);
    const auto single=selectMeshPointsOnScreen(object.get(),camera,MeshScreenCircle{{25,75},1});
    CHECK(single.masks[0].weights==std::vector<double>({1,0,0,0,0}));
}
class Depth final:public MeshScreenDepthSampler
{
  public:
    explicit Depth(std::optional<double> value):value(value){}
    std::optional<double> sample(MeshScreenPoint)const override{return value;}
    std::optional<double> value;
};
void depth()
{
    Object3DEngine engine;const auto object=engine.createObject(fixture());Depth buffer(.5);
    MeshScreenSelectionOptions options;options.depthSampler=&buffer;
    const MeshScreenRectangle all{{10,20},{110,120}};
    const auto front=selectMeshPointsOnScreen(object.get(),view(),all,options);
    CHECK(front.masks[0].weights==std::vector<double>({1,0,0,1,0,0}));
    CHECK(front.depthQueries==4&&front.occludedPoints==2);
    auto reverse=view();reverse.reversedDepth=true;
    const auto reversed=selectMeshPointsOnScreen(object.get(),reverse,all,options);
    CHECK(reversed.masks[0].weights==std::vector<double>({0,1,1,0,0,0}));
    buffer.value=.4-1e-6;
    const auto tolerant=selectMeshPointsOnScreen(object.get(),view(),all,options);
    CHECK(tolerant.selectedPoints==2);options.depthTolerance=0;
    CHECK(selectMeshPointsOnScreen(object.get(),view(),all,options).selectedPoints==0);
    options.depthTolerance=1e-5;
    buffer.value.reset();rejects([&]{(void)selectMeshPointsOnScreen(object.get(),view(),all,options);});
    options.missingDepth=MeshScreenMissingDepth::Exclude;
    const auto missing=selectMeshPointsOnScreen(object.get(),view(),all,options);
    CHECK(missing.missingDepthPoints==4&&missing.selectedPoints==0);
    options.missingDepth=MeshScreenMissingDepth::Include;
    const auto included=selectMeshPointsOnScreen(object.get(),view(),all,options);
    CHECK(included.missingDepthPoints==4&&included.selectedPoints==4);
    buffer.value=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{(void)selectMeshPointsOnScreen(object.get(),view(),all,options);});
    buffer.value=1.1;rejects([&]{(void)selectMeshPointsOnScreen(object.get(),view(),all,options);});
}
void overrides()
{
    Object3DEngine engine;auto data=fixture();auto copy=data.meshes[0];copy.name="Other";data.meshes.push_back(std::move(copy));
    const auto object=engine.createObject(std::move(data));
    std::vector<Vector3> positions;for(const auto &point:object.get().data().meshes[0].topology->points)positions.push_back(point.position);
    positions[0]={3,0,0};positions[4]={0,0,0};const std::array overrides{MeshScreenMeshPositions{0,positions}};
    const std::array order{std::size_t{1},std::size_t{0}};MeshScreenSelectionOptions options;
    options.meshes=order;options.positions=overrides;options.includeUnusedPoints=true;
    const auto result=selectMeshPointsOnScreen(object.get(),view(),MeshScreenRectangle{{10,20},{110,120}},options);
    CHECK(result.masks.size()==2&&result.masks[0].mesh==0&&result.masks[1].mesh==1);
    CHECK(result.masks[0].weights==std::vector<double>({0,1,1,1,1,0}));
    CHECK(result.masks[1].weights==std::vector<double>({1,1,1,1,1,0}));
    CHECK(result.clippedPoints==3&&result.selectedPoints==9);
}
void deformed()
{
    auto data=fixture();data.kind=Object3DKind::SkeletalMesh;
    data.bones={{"root",NoBone,{}},{"move",0,{}}};
    auto &topology=*data.meshes[0].topology;
    for(auto &point:topology.points)point.weights={{0,1}};
    topology.points[0].weights={{1,1}};
    Object3DEngine engine;const auto object=engine.createObject(std::move(data));auto actor=object.get().instantiate();
    auto local=actor->pose().locals()[1];local.position.x=.4;actor->pose().setLocal(1,local);
    SkeletalMeshComponent *component=nullptr;
    for(auto *part:actor->components())if(auto *value=dynamic_cast<SkeletalMeshComponent*>(part)){component=value;break;}
    CHECK(component);const auto &mesh=object.get().data().meshes[0];const auto deformed=component->deformedVertices();
    const auto snapshot=captureMeshScreenPositions(mesh,deformed);
    CHECK(snapshot.positions.size()==6&&snapshot.available.size()==6&&snapshot.available[0]==1);
    CHECK(snapshot.available[4]==0&&snapshot.available[5]==0);
    const std::array overrides{MeshScreenMeshPositions{0,snapshot.positions,snapshot.available}};
    MeshScreenSelectionOptions options;options.positions=overrides;
    const auto result=selectMeshPointsOnScreen(object.get(),view(),MeshScreenCircle{{40,110},1},options);
    CHECK(result.masks[0].weights==std::vector<double>({1,0,0,0,0,0}));
    const auto rest=selectMeshPointsOnScreen(object.get(),view(),MeshScreenCircle{{40,110},1});
    CHECK(rest.selectedPoints==0);
    options.includeUnusedPoints=true;
    const auto all=selectMeshPointsOnScreen(object.get(),view(),MeshScreenRectangle{{10,20},{110,120}},options);
    CHECK(all.unavailablePoints==2&&all.selectedPoints==4);
    auto inconsistent=std::vector<DeformedVertex>(deformed.begin(),deformed.end());
    const auto &definition=mesh.topology->layout->definition();
    const auto pointFor=[&](std::size_t vertex){return definition.layouts[definition.renderToLayout[vertex]].position;};
    bool altered=false;
    for(std::size_t i=0;i<inconsistent.size()&&!altered;++i)
        for(std::size_t j=0;j<i;++j)if(pointFor(i)==pointFor(j))
        {inconsistent[i].position.x+=.2;altered=true;break;}
    CHECK(altered);rejects([&]{(void)captureMeshScreenPositions(mesh,inconsistent);});
    rejects([&]{(void)captureMeshScreenPositions(mesh,deformed.first(deformed.size()-1));});
    auto renderOnly=mesh;renderOnly.topology.reset();
    const auto renderSnapshot=captureMeshScreenPositions(renderOnly,deformed);
    CHECK(renderSnapshot.positions.size()==deformed.size());
    CHECK(std::all_of(renderSnapshot.available.begin(),renderSnapshot.available.end(),[](auto value){return value==1;}));
    for(std::size_t i=0;i<deformed.size();++i)CHECK(renderSnapshot.positions[i]==deformed[i].position);
    MeshScreenCaptureOptions limits;limits.maximumPoints=5;
    rejects([&]{(void)captureMeshScreenPositions(mesh,deformed,limits);});
    limits={};limits.maximumRenderVertices=1;
    rejects([&]{(void)captureMeshScreenPositions(mesh,deformed,limits);});
}
void editor()
{
    Object3DEngine engine;const auto object=engine.createObject(fixture());const auto editor=engine.editObject(object);
    const auto before=editor.get().revision();
    const auto result=selectMeshPointsOnScreen(editor.get().object().get(),view(),MeshScreenRectangle{{15,25},{25,115}});
    CHECK(editor.get().revision()==before&&editor.get().object()==object);
    editor.get().setPointSelection("screen",result.masks);CHECK(editor.get().pointSelection("screen")[0]==result.masks[0]);
    CHECK(editor.get().undo()&&editor.get().pointSelectionNames().empty());
    CHECK(editor.get().redo()&&editor.get().pointSelection("screen")[0]==result.masks[0]);
    MeshBrushSettings brush;brush.falloff=MeshBrushFalloff::Constant;
    brush.mask=editor.get().pointSelection("screen")[0].weights;
    const SculptStrokeSample stroke{{{0,0,0},10,1},{0,0,.2}};
    const auto sculpted=editor.get().sculptStroke(0,std::span(&stroke,1),brush);
    CHECK(sculpted.changedPoints==2);
    const auto &changed=editor.get().object().get().data().meshes[0].topology->points;
    CHECK(std::abs(changed[0].position.z)<1e-9&&std::abs(changed[3].position.z)<1e-9);
    CHECK(std::abs(changed[1].position.z-.5)<1e-9&&std::abs(changed[2].position.z-.5)<1e-9);
    CHECK(editor.get().undo()&&editor.get().redo());
    const auto bytes=exportObject3DProject(editor);Object3DEngine restored;
    const auto project=importObject3DProject(restored.reference(),bytes);
    CHECK(project.editor.get().pointSelection("screen")[0]==result.masks[0]);
#if IIACTOROBJECT_HAS_NSCHARACTER
    NSCharacter::CharacterEditor character(editor);CHECK(character.pointSelection("screen")[0]==result.masks[0]);
#endif
}
void validation()
{
    Object3DEngine engine;const auto object=engine.createObject(fixture());const auto editor=engine.editObject(object);
    const auto revision=editor.get().revision();const MeshScreenRectangle all{{0,0},{200,200}};
    auto camera=view();camera.viewport.width=0;
    rejects([&]{(void)selectMeshPointsOnScreen(object.get(),camera,all);});
    camera=view();camera.clipFromObject[0]=std::numeric_limits<double>::infinity();
    rejects([&]{(void)selectMeshPointsOnScreen(object.get(),camera,all);});
    rejects([&]{(void)selectMeshPointsOnScreen(object.get(),view(),MeshScreenCircle{{0,0},-1});});
    const std::array shortPolygon{MeshScreenPoint{0,0},MeshScreenPoint{1,1}};
    rejects([&]{(void)selectMeshPointsOnScreen(object.get(),view(),MeshScreenPolygon{shortPolygon});});
    const std::array duplicates{std::size_t{0},std::size_t{0}};MeshScreenSelectionOptions options;options.meshes=duplicates;
    rejects([&]{(void)selectMeshPointsOnScreen(object.get(),view(),all,options);});
    options={};options.maximumPoints=5;rejects([&]{(void)selectMeshPointsOnScreen(object.get(),view(),all,options);});
    options={};options.maximumTriangleIndices=2;rejects([&]{(void)selectMeshPointsOnScreen(object.get(),view(),all,options);});
    const std::array polygon{MeshScreenPoint{0,0},MeshScreenPoint{200,0},MeshScreenPoint{100,200}};
    options={};options.maximumPolygonEdgeTests=1;
    rejects([&]{(void)selectMeshPointsOnScreen(object.get(),view(),MeshScreenPolygon{polygon},options);});
    std::vector<Vector3> bad(3);const std::array invalid{MeshScreenMeshPositions{0,bad}};
    options={};options.positions=invalid;rejects([&]{(void)selectMeshPointsOnScreen(object.get(),view(),all,options);});
    bad.resize(6);bad[0].x=std::numeric_limits<double>::quiet_NaN();
    const std::array invalidFinite{MeshScreenMeshPositions{0,bad}};options.positions=invalidFinite;
    rejects([&]{(void)selectMeshPointsOnScreen(object.get(),view(),all,options);});
    bad[0].x=0;std::vector<std::uint8_t> availability(6,1);availability[1]=2;
    const std::array invalidAvailability{MeshScreenMeshPositions{0,bad,availability}};options.positions=invalidAvailability;
    rejects([&]{(void)selectMeshPointsOnScreen(object.get(),view(),all,options);});
    CHECK(editor.get().revision()==revision&&editor.get().object()==object&&engine.objectCount()==1);
}
#if IIACTOROBJECT_HAS_NSCHARACTER
void human(const char *directory)
{
    Object3DEngine engine;const auto model=NSCharacter::loadMakeHumanTemplate(engine.reference(),directory);
    const auto character=model.generate();const auto object=character->object();
    const auto &mesh=object.get().data().meshes.at(0);CHECK(mesh.topology&&mesh.topology->points.size()>19'000);
    auto actor=object.get().instantiate();auto root=actor->pose().locals()[0];root.position.x+=.05;actor->pose().setLocal(0,root);
    SkeletalMeshComponent *component=nullptr;
    for(auto *part:actor->components())if(auto *value=dynamic_cast<SkeletalMeshComponent*>(part)){component=value;break;}
    CHECK(component);const auto captured=captureMeshScreenPositions(mesh,component->deformedVertices());
    double xExtent=0,yExtent=0,zExtent=0;
    for(std::size_t i=0;i<captured.positions.size();++i)if(captured.available[i])
    {
        xExtent=std::max(xExtent,std::abs(captured.positions[i].x));
        yExtent=std::max(yExtent,std::abs(captured.positions[i].y));
        zExtent=std::max(zExtent,std::abs(captured.positions[i].z));
    }
    CHECK(xExtent>0&&yExtent>0&&zExtent>0);
    MeshScreenView camera;camera.viewport={0,0,1000,1000,true};
    camera.clipFromObject[0]=.95/xExtent;camera.clipFromObject[5]=.95/yExtent;camera.clipFromObject[10]=.95/zExtent;
    const std::array overrides{MeshScreenMeshPositions{0,captured.positions,captured.available}};
    MeshScreenSelectionOptions options;options.positions=overrides;
    const MeshScreenCircle circle{{500,500},250};
    const auto result=selectMeshPointsOnScreen(object.get(),camera,circle,options);
    std::vector<bool> visible(captured.positions.size());const auto &definition=mesh.topology->layout->definition();
    for(const auto vertex:mesh.geometry.triangles)
        visible[definition.layouts[definition.renderToLayout[vertex]].position]=true;
    std::size_t expected=0;
    for(std::size_t i=0;i<captured.positions.size();++i)if(visible[i]&&captured.available[i])
    {
        const auto p=captured.positions[i];const auto x=500+500*camera.clipFromObject[0]*p.x;
        const auto y=500-500*camera.clipFromObject[5]*p.y;
        const bool inside=std::hypot(x-500,y-500)<=250+options.edgeTolerance;
        CHECK(result.masks[0].weights[i]==(inside?1:0));expected+=inside;
    }
    CHECK(expected>1000&&expected==result.selectedPoints);
    const auto editor=engine.editObject(object);editor.get().setPointSelection("human-screen",result.masks);
    CHECK(editor.get().pointSelection("human-screen")[0]==result.masks[0]);
    std::cout<<"Human source points="<<captured.positions.size()<<" selected="<<expected<<'\n';
}
#endif
int main(int argc,char **argv)
{
    try{
#if IIACTOROBJECT_HAS_NSCHARACTER
        if(argc==3&&std::string_view(argv[1])=="human"){human(argv[2]);return 0;}
#endif
        if(argc!=2)return 2;const std::string_view mode=argv[1];
        if(mode=="shapes")shapes();else if(mode=="perspective")perspective();else if(mode=="depth")depth();
        else if(mode=="overrides")overrides();else if(mode=="deformed")deformed();
        else if(mode=="editor")editor();else if(mode=="validation")validation();
        else return 2;return 0;
    }catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
