#include <iiActorObject/MeshCollider.h>
#include <iiActorObject/MeshSurface.h>
#include <iiActorObject/SurfaceFitting.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
bool near(double a,double b,double e=1e-9){return std::abs(a-b)<e;}
struct Shape final{std::vector<Vector3> points;std::vector<std::uint32_t> triangles;};
Shape cube(double r=1)
{
    return {{{-r,-r,-r},{r,-r,-r},{r,r,-r},{-r,r,-r},{-r,-r,r},{r,-r,r},{r,r,r},{-r,r,r}},
        {0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7}};
}
class Plane final:public SurfaceCollider
{
  public:
    SurfaceCollisionSample sample(Vector3 p,SurfaceQueryBudget &budget) const override
    {budget.visitNode();return {p.z,{0,0,1},{}};}
    SurfaceCollisionContact triangleContact(const std::array<Vector3,3> &p,double maximum,SurfaceQueryBudget &budget) const override
    {
        budget.visitNode();budget.testTriangle();std::array<double,3> bary{};bool found=false;
        for(unsigned i=0;i<3&&!found;++i)for(unsigned j=i+1;j<3&&!found;++j)if((p[i].z<0&&p[j].z>=0)||(p[j].z<0&&p[i].z>=0))
        {const auto t=p[i].z/(p[i].z-p[j].z);bary[i]=1-t;bary[j]=t;found=true;}
        if(!found){unsigned best=0;for(unsigned i=1;i<3;++i)if(std::abs(p[i].z)<std::abs(p[best].z))best=i;bary[best]=1;}
        Vector3 target;for(unsigned i=0;i<3;++i){target.x+=bary[i]*p[i].x;target.y+=bary[i]*p[i].y;target.z+=bary[i]*p[i].z;}
        const auto d=std::abs(target.z);if(d>maximum)return {};return {d,{target.x,target.y,0},target,bary};
    }
};
Object3DData patchData()
{
    auto data=parseObject3DObj("v 0 0 -.01\nv .1 0 -.01\nv .1 .1 -.01\nv 0 .1 -.01\nv .5 .5 -.2\nf 1 2 3 4\n");
    Object3DEngine engine;return engine.createObject(std::move(data)).get().data();
}
void trianglePairs()
{
    const std::array source{Vector3{0,0,0},Vector3{1,0,0},Vector3{0,1,0}};MeshSurface surface(source,std::array<std::uint32_t,3>{0,1,2});
    auto hit=surface.closestTriangle(std::array{Vector3{0,0,2},Vector3{1,0,2},Vector3{0,1,2}});CHECK(hit.valid()&&near(hit.source.distance,2)&&near(hit.targetPosition.z,2));
    CHECK(!surface.closestTriangle(std::array{Vector3{0,0,2},Vector3{1,0,2},Vector3{0,1,2}},1).valid());
    hit=surface.closestTriangle(std::array{Vector3{.2,.2,-1},Vector3{.2,.2,1},Vector3{.8,.2,0}});CHECK(hit.valid()&&near(hit.source.distance,0));
    hit=surface.closestTriangle(std::array{Vector3{-.2,.5,0},Vector3{.8,.5,0},Vector3{.3,-.5,0}},0);CHECK(hit.valid()&&near(hit.source.distance,0));
    rejects([&]{(void)surface.closestTriangle(std::array{Vector3{},Vector3{},Vector3{1,1,1}});});
}
void cubeDistance()
{
    auto s=cube();MeshCollider collider(s.points,s.triangles);CHECK(collider.statistics().components==1&&collider.statistics().triangles==12);
    CHECK(near(collider.sample({0,0,0}).signedDistance,-1)&&near(collider.sample({2,0,0}).signedDistance,1));
    CHECK(near(collider.sample({2,2,2}).signedDistance,std::sqrt(3.))&&near(collider.sample({1,1,1}).signedDistance,0));
    for(std::size_t i=0;i<s.triangles.size();i+=9)std::swap(s.triangles[i],s.triangles[i+1]);MeshCollider repaired(s.points,s.triangles);CHECK(repaired.statistics().reorientedTriangles>0&&near(repaired.sample({0,0,0}).signedDistance,-1));
    Object3DMesh seams;seams.name="Seams";for(std::size_t i=0;i<s.triangles.size();++i){seams.geometry.vertices.push_back({s.points[s.triangles[i]],{0,0,1},{}});seams.geometry.triangles.push_back(static_cast<std::uint32_t>(i));}
    auto welded=MeshCollider::fromMesh(seams);CHECK(welded.statistics().inputVertices==36&&welded.statistics().collisionVertices==8&&near(welded.sample({0,0,0}).signedDistance,-1));
}
void nested()
{
    auto s=cube(),inside=cube(.25);for(auto p:inside.points)s.points.push_back(p);for(auto p:inside.triangles)s.triangles.push_back(p+8);
    MeshCollider collider(s.points,s.triangles);CHECK(collider.statistics().components==2);CHECK(near(collider.sample({0,0,0}).signedDistance,.25));CHECK(near(collider.sample({.5,0,0}).signedDistance,-.25));CHECK(near(collider.sample({2,0,0}).signedDistance,1));
}
void concave()
{
    const std::array xy{std::array{-1.,-1.},std::array{1.,-1.},std::array{1.,0.},std::array{0.,0.},std::array{0.,1.},std::array{-1.,1.}};Shape s;
    for(double z:{-1.,1.})for(auto p:xy)s.points.push_back({p[0],p[1],z});
    for(std::uint32_t i=1;i<5;++i){s.triangles.insert(s.triangles.end(),{0,i+1,i,6,6+i,7+i});}
    for(std::uint32_t i=0;i<6;++i){auto j=(i+1)%6;s.triangles.insert(s.triangles.end(),{i,j,j+6,i,j+6,i+6});}
    MeshCollider collider(s.points,s.triangles);
    for(unsigned i=0;i<600;++i)
    {
        const Vector3 p{1.5*std::sin(i*.131+.09),1.5*std::cos(i*.173+.21),1.5*std::sin(i*.217+.4)};double d=1e10;
        for(unsigned j=0;j<6;++j){const auto a=xy[j],b=xy[(j+1)%6];const auto x=b[0]-a[0],y=b[1]-a[1];const auto t=std::clamp(((p.x-a[0])*x+(p.y-a[1])*y)/(x*x+y*y),0.,1.);d=std::min(d,std::hypot(p.x-a[0]-t*x,p.y-a[1]-t*y));}
        if(p.x>-1&&p.x<1&&p.y>-1&&p.y<1&&(p.x<0||p.y<0))d=-d;const auto z=std::abs(p.z)-1;
        const auto expected=std::min(std::max(d,z),0.)+std::hypot(std::max(d,0.),std::max(z,0.));CHECK(near(collider.sample(p).signedDistance,expected));
    }
}
void invalid()
{
    auto s=cube();MeshCollider valid(s.points,s.triangles);CHECK(valid.statistics().components==1);
    auto open=s.triangles;open.resize(open.size()-3);rejects([&]{MeshCollider c(s.points,open);});
    auto duplicated=s.triangles;duplicated.insert(duplicated.end(),s.triangles.begin(),s.triangles.begin()+3);rejects([&]{MeshCollider c(s.points,duplicated);});
    MeshColliderOptions options;options.maximumPoints=1;rejects([&]{MeshCollider c(s.points,s.triangles,options);});
}
void fitting()
{
    Plane plane;auto mesh=patchData().meshes[0];const auto layout=mesh.topology->layout;const auto unused=mesh.topology->points[4].position;
    TopologyMorph detail;detail.name="Detail";detail.sourceTarget=7;detail.deltas={{0,{0,0,.001}}};mesh.topology->morphTargets={detail};
    auto result=fitMeshSurface(mesh,plane);CHECK(result.converged&&result.changedPoints==4&&result.pointViolations==0&&result.triangleViolations==0);
    for(unsigned i=0;i<4;++i)CHECK(near(mesh.topology->points[i].position.z,.002));CHECK(mesh.topology->points[4].position==unused&&mesh.topology->layout==layout&&mesh.topology->morphTargets[0].sourceTarget==7);
    CHECK(fitMeshSurface(mesh,plane).changedPoints==0);
    for(unsigned i=0;i<4;++i)mesh.topology->points[i].position.z=.002-.5e-6;const auto close=fitMeshSurface(mesh,plane);CHECK(close.changedPoints==0&&near(close.minimumTriangleClearance,.002-.5e-6,1e-12));
}
void masks()
{
    Plane plane;auto mesh=patchData().meshes[0];const auto before=mesh.topology->points[0].position;const std::array mobility{0.,1.,1.,1.,0.};SurfaceFitOptions options;options.mobility=mobility;
    rejects([&]{(void)fitMeshSurface(mesh,plane,options);});CHECK(mesh.topology->points[0].position==before);
    options.allowUnresolved=true;auto result=fitMeshSurface(mesh,plane,options);CHECK(!result.converged&&result.pinnedViolations>0&&mesh.topology->points[0].position==before);
}
void faceConstraints()
{
    auto s=cube(.25);MeshCollider collider(s.points,s.triangles);auto data=parseObject3DObj("v -1 -1 0\nv 1 -1 0\nv 0 1 0\nf 1 2 3\n");Object3DEngine engine;auto mesh=engine.createObject(data).get().data().meshes[0];
    const std::array mobility{0.,0.,0.};SurfaceFitOptions options;options.mobility=mobility;options.allowUnresolved=true;
    auto result=fitMeshSurface(mesh,collider,options);CHECK(result.pointViolations==0&&result.triangleViolations>0&&!result.converged);options.allowUnresolved=false;rejects([&]{(void)fitMeshSurface(mesh,collider,options);});
}
void history()
{
    Plane plane;Object3DEngine engine;Object3DTemplateDefinition definition;definition.base=engine.createObject(patchData());definition.targets={{"shape",0,{{2,{.1,0,0}}}}};auto editor=engine.editTemplate(engine.createTemplate(definition));
    CHECK(editor.get().fitSurface(0,plane).converged);CHECK(editor.get().layers().size()==1);editor.get().setTemplateValues(std::array{TemplateValue{"shape",.5}});
    const auto bytes=exportObject3DProject(editor);CHECK(bytes[8]==8);Object3DEngine destination;auto restored=importObject3DProject(destination.reference(),bytes);CHECK(restored.editor.get().undo()&&restored.editor.get().redo());CHECK(near(restored.editor.get().object().get().data().meshes[0].topology->points[2].position.z,.002));
    const auto revision=editor.get().revision();CHECK(editor.get().fitSurface(0,plane).changedPoints==0&&editor.get().revision()==revision);
}
void budgets()
{
    auto s=cube();MeshCollider collider(s.points,s.triangles);SurfaceQueryBudget budget;budget.maximumNodeVisits=1;rejects([&]{(void)collider.sample({0,0,0},budget);});
    Plane plane;auto mesh=patchData().meshes[0];const auto before=mesh.topology->points[0].position;SurfaceFitOptions options;options.maximumNodeVisits=1;rejects([&]{(void)fitMeshSurface(mesh,plane,options);});CHECK(mesh.topology->points[0].position==before);
    options=SurfaceFitOptions{};options.maximumDisplacement=.001;rejects([&]{(void)fitMeshSurface(mesh,plane,options);});CHECK(mesh.topology->points[0].position==before);
    options=SurfaceFitOptions{};options.maximumPoints=1;rejects([&]{(void)fitMeshSurface(mesh,plane,options);});CHECK(mesh.topology->points[0].position==before);
    options=SurfaceFitOptions{};options.maximumTriangles=1;rejects([&]{(void)fitMeshSurface(mesh,plane,options);});CHECK(mesh.topology->points[0].position==before);
    options=SurfaceFitOptions{};options.maximumPointVisits=1;rejects([&]{(void)fitMeshSurface(mesh,plane,options);});CHECK(mesh.topology->points[0].position==before);
    MeshColliderOptions preprocessing;preprocessing.maximumPreprocessVisits=1;rejects([&]{MeshCollider limited(s.points,s.triangles,preprocessing);});
}
void escape()
{
    auto a=cube(),b=cube();for(auto &p:a.points)p={p.x*.49975-.50025,p.y*.01,p.z*.01};for(auto &p:b.points)p={p.x*.49975+.50025,p.y*.01,p.z*.01};
    MeshCollider left(a.points,a.triangles),right(b.points,b.triangles);const std::array<const SurfaceCollider*,2> obstacles{&left,&right};
    auto mesh=parseObject3DObj("v 0 -.001 -.001\nv 0 .001 -.001\nv 0 0 .001\nf 1 2 3\n").meshes[0];Object3DEngine engine;mesh=engine.createObject(Object3DData{.meshes={mesh}}).get().data().meshes[0];
    auto trapped=mesh;SurfaceFitOptions options;options.edgeStiffness=0;options.maximumDisplacement=.1;options.escapeConcavities=false;options.allowUnresolved=true;
    CHECK(!fitMeshSurface(trapped,obstacles,options).converged);options.escapeConcavities=true;options.allowUnresolved=false;
    const auto result=fitMeshSurface(mesh,obstacles,options);CHECK(result.converged&&result.escapeTrials>0&&result.escapePointProjections+result.escapeTriangleProjections>0&&result.maximumDisplacement<=.1);
    CHECK(result.minimumTriangleClearance>=options.clearance-options.tolerance);
}
void boundaryGradient()
{
    auto shape=cube();for(auto &p:shape.points)p={p.x*.49975-.50025,p.y*.01,p.z*.01};MeshCollider collider(shape.points,shape.triangles);
    for(unsigned i=1;i<300;++i)
    {
        const auto g=collider.sample({shape.points[1].x,.005*std::sin(i*.217),.005*std::cos(i*.131)}).gradient;
        CHECK(g.x>.999999999&&std::abs(g.y)<1e-9&&std::abs(g.z)<1e-9);
    }
}
int main(int argc,char **argv)
{
    try{if(argc!=2)return 2;const std::string c=argv[1];if(c=="triangle_pairs")trianglePairs();else if(c=="cube")cubeDistance();else if(c=="nested")nested();else if(c=="concave")concave();else if(c=="invalid")invalid();else if(c=="fitting")fitting();else if(c=="masks")masks();else if(c=="face_constraints")faceConstraints();else if(c=="history")history();else if(c=="budgets")budgets();else if(c=="escape")escape();else if(c=="boundary_gradient")boundaryGradient();else return 2;}
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
