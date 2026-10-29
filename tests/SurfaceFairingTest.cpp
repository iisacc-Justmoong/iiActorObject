#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
template<class F>void rejects(F f){try{f();}catch(const std::exception &){return;}throw std::runtime_error("Expected rejection");}
Object3DMesh box(double x,double z,double center=0)
{
    std::ostringstream out;for(auto p:std::array{Vector3{-x,-x,.95},Vector3{x,-x,.95},Vector3{x,x,.95},Vector3{-x,x,.95},Vector3{-x,-x,z},Vector3{x,-x,z},Vector3{x,x,z},Vector3{-x,x,z}})out<<"v "<<p.x+center<<' '<<p.y+center<<' '<<p.z<<'\n';
    out<<"f 1 4 3 2\nf 5 6 7 8\nf 1 2 6 5\nf 2 3 7 6\nf 3 4 8 7\nf 4 1 5 8\n";return parseObject3DObj(out.str()).meshes[0];
}
Object3DMesh grid()
{
    std::ostringstream out;for(unsigned y=0;y<7;++y)for(unsigned x=0;x<7;++x)out<<"v "<<(int(x)-3)*.025<<' '<<(int(y)-3)*.025<<" 1.001\n";
    for(unsigned y=0;y<6;++y)for(unsigned x=0;x<6;++x){const auto p=y*7+x+1;out<<"f "<<p<<' '<<p+1<<' '<<p+8<<' '<<p+7<<'\n';}
    auto mesh=parseObject3DObj(out.str()).meshes[0];TopologyMorph morph;morph.name="Detail";morph.sourceTarget=42;morph.deltas={{2,{0,0,.001}}};mesh.topology->morphTargets.push_back(morph);return mesh;
}
double roughness(const Object3DMesh &before,const Object3DMesh &after)
{
    auto sub=[](Vector3 a,Vector3 b){return Vector3{a.x-b.x,a.y-b.y,a.z-b.z};};
    double energy=0;auto d=[&](unsigned p){return sub(after.topology->points[p].position,before.topology->points[p].position);};
    for(unsigned y=0;y<7;++y)for(unsigned x=0;x<7;++x){auto p=y*7+x;if(x<6){auto v=sub(d(p),d(p+1));energy+=v.x*v.x+v.y*v.y+v.z*v.z;}if(y<6){auto v=sub(d(p),d(p+7));energy+=v.x*v.x+v.y*v.y+v.z*v.z;}}
    return energy;
}
struct Scene
{
    Object3DMesh original=grid();
    MeshCollider base=MeshCollider::fromMesh(box(1,1)),bump=MeshCollider::fromMesh(box(.02,1.02));
    std::array<const SurfaceCollider*,2> obstacles{&base,&bump};
    SurfaceFitOptions options;
    Scene(){options.edgeStiffness=0;options.tolerance=1e-5;options.iterations=128;options.maximumNodeVisits=options.maximumTriangleTests=64'000'000;}
    SurfaceFitOptions quality(){auto o=options;o.fairing.iterations=24;return o;}
};
void energy()
{
    Scene s;auto baseline=s.original,quality=s.original;const auto a=fitMeshSurface(baseline,s.obstacles,s.options),b=fitMeshSurface(quality,s.obstacles,s.quality());
    CHECK(a.converged&&b.converged&&b.fairing.accepted>0&&b.fairing.energyBefore>0&&b.fairing.energyAfter<b.fairing.energyBefore);
    CHECK(roughness(s.original,quality)<roughness(s.original,baseline)*.95);CHECK(b.pointViolations==0&&b.triangleViolations==0&&b.maximumDisplacement<=s.options.maximumDisplacement);
    for(const auto &p:quality.topology->points)CHECK(p.position.z>=1.002-s.options.tolerance-1e-12);
    CHECK(quality.topology->layout==s.original.topology->layout&&quality.topology->morphTargets[0].sourceTarget==42&&quality.topology->points.size()==49);
    CHECK(quality.topology->textureCoordinates==s.original.topology->textureCoordinates);
    std::cout<<"roughnessBefore="<<roughness(s.original,baseline)<<" roughnessAfter="<<roughness(s.original,quality)<<" accepted="<<b.fairing.accepted<<'\n';
}
void pins()
{
    Scene s;s.original.topology->points[0].position.z=1.002;std::vector<double> mass(49,1);mass[0]=0;auto o=s.quality();o.mobility=mass;
    auto mesh=s.original;const auto result=fitMeshSurface(mesh,s.obstacles,o);CHECK(result.converged&&result.fairing.accepted>0&&mesh.topology->points[0].position==s.original.topology->points[0].position);
}
void strain()
{
    Scene s;std::ostringstream input;const std::array<double,7> x{-.075,-.05,-.025,0,.0001,.05,.075};
    for(unsigned y=0;y<7;++y)for(unsigned i=0;i<7;++i)input<<"v "<<x[i]<<' '<<(int(y)-3)*.025<<" 1.001\n";
    for(unsigned y=0;y<6;++y)for(unsigned i=0;i<6;++i){auto p=y*7+i+1;input<<"f "<<p<<' '<<p+1<<' '<<p+8<<' '<<p+7<<'\n';}
    auto baseline=parseObject3DObj(input.str()).meshes[0],quality=baseline,legacy=baseline;
    auto bump=MeshCollider::fromMesh(box(.02,1.02,-.05));std::array<const SurfaceCollider*,2> obstacles{&s.base,&bump};
    const auto a=fitMeshSurface(baseline,obstacles,s.options),b=fitMeshSurface(quality,obstacles,s.quality());
    CHECK(a.converged&&b.converged&&b.fairing.accepted>0&&b.fairing.energyAfter<b.fairing.energyBefore);
    std::cout<<"baselineMaximumEdgeStrain="<<a.maximumEdgeStrain<<" fairingMaximumEdgeStrain="<<b.maximumEdgeStrain<<'\n';
    CHECK(b.maximumEdgeStrain<=a.maximumEdgeStrain+1e-10);
    auto old=s.quality();old.fairing.shapePolicy=SurfaceFairingShapePolicy::LegacyDisplacement;
    const auto c=fitMeshSurface(legacy,obstacles,old);CHECK(c.converged&&c.maximumEdgeStrain>a.maximumEdgeStrain);
}
void budgets()
{
    Scene s;auto mesh=s.original;const auto baseline=fitMeshSurface(mesh,s.obstacles,s.options);auto o=s.quality();o.maximumPointVisits=baseline.pointVisits+1;mesh=s.original;
    rejects([&]{(void)fitMeshSurface(mesh,s.obstacles,o);});for(std::size_t p=0;p<49;++p)CHECK(mesh.topology->points[p].position==s.original.topology->points[p].position);
}
void noop()
{
    Scene s;auto mesh=s.original;(void)fitMeshSurface(mesh,s.obstacles,s.options);const auto before=mesh;const auto result=fitMeshSurface(mesh,s.obstacles,s.quality());
    CHECK(result.changedPoints==0&&result.fairing.accepted==0);for(std::size_t p=0;p<49;++p)CHECK(mesh.topology->points[p].position==before.topology->points[p].position);
}
void validation()
{
    Scene s;for(unsigned kind=0;kind<6;++kind){auto o=s.quality();if(kind==0)o.fairing.strength=std::numeric_limits<double>::quiet_NaN();if(kind==1)o.fairing.referenceWeight=-.1;if(kind==2)o.fairing.iterations=257;if(kind==3)o.fairing.projectionIterations=0;if(kind==4)o.fairing.lineSearchSteps=0;if(kind==5)o.fairing.shapePolicy=static_cast<SurfaceFairingShapePolicy>(99);rejects([&]{validateSurfaceFitOptions(o,49,72);});}
}
void settings()
{Scene s;SurfaceFitSettings a(s.options),b(s.quality());CHECK(a!=b);auto c=b;CHECK(c==b);}
void project(const std::string &filename,bool legacy,const std::filesystem::path &outputPath={})
{
    std::ifstream file(filename,std::ios::binary);CHECK(file.good());const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});CHECK(bytes[8]==10);
    Object3DEngine engine;auto source=importObject3DProject(engine.reference(),bytes);auto &e=source.editor.get();CHECK(e.surfaceFittings().size()==1&&e.surfaceFittings()[0].fitting.options().fairing.iterations==0);
    if(legacy)
    {
        CHECK(exportObject3DProject(source.editor)[8]==10);e.setTemplateValues(std::array{TemplateValue{"body-up",.5}});CHECK(std::abs(e.object().get().data().meshes[1].topology->points[0].position.z-1.022)<1e-9);return;
    }
    auto rule=e.surfaceFittings()[0];auto o=rule.fitting.options();o.fairing={8,.7,.05,16,4};rule.fitting=SurfaceFitSettings(o);e.setSurfaceFittings(std::span(&rule,1));CHECK(e.surfaceFittings()[0]==rule);
    const auto output=exportObject3DProject(source.editor);CHECK(output[8]==12);Object3DEngine restored;auto restoredProject=importObject3DProject(restored.reference(),output);
    CHECK(restoredProject.editor.get().surfaceFittings()[0]==rule&&restoredProject.editor.get().undo()&&restoredProject.editor.get().redo());
    CHECK(!outputPath.empty());std::filesystem::create_directories(outputPath.parent_path());std::ofstream out(outputPath,std::ios::binary);out.write(reinterpret_cast<const char*>(output.data()),output.size());CHECK(out.good());
    Object3DEngine inactiveEngine;auto inactive=importObject3DProject(inactiveEngine.reference(),bytes);auto &inactiveEditor=inactive.editor.get();auto inactiveRule=inactiveEditor.surfaceFittings()[0];auto inactiveOptions=inactiveRule.fitting.options();
    CHECK(inactiveOptions.fairing.iterations==0);inactiveOptions.fairing.shapePolicy=SurfaceFairingShapePolicy::LegacyDisplacement;inactiveRule.fitting=SurfaceFitSettings(inactiveOptions);inactiveEditor.setSurfaceFittings(std::span(&inactiveRule,1));
    const auto inactiveBytes=exportObject3DProject(inactive.editor);CHECK(inactiveBytes[8]==12);Object3DEngine inactiveRestored;auto both=importObject3DProject(inactiveRestored.reference(),inactiveBytes);
    CHECK(both.editor.get().surfaceFittings()[0]==inactiveRule&&both.editor.get().undo());CHECK(both.editor.get().surfaceFittings()[0].fitting.options().fairing.shapePolicy==SurfaceFairingShapePolicy::PreserveMaximumEdgeStrain);
}
void legacyFairing(const std::string &filename)
{
    std::ifstream file(filename,std::ios::binary);CHECK(file.good());const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});CHECK(bytes[8]==11);
    Object3DEngine engine;auto project=importObject3DProject(engine.reference(),bytes);auto &editor=project.editor.get();CHECK(editor.surfaceFittings()[0].fitting.options().fairing.shapePolicy==SurfaceFairingShapePolicy::LegacyDisplacement);
    CHECK(exportObject3DProject(project.editor)==bytes);auto rule=editor.surfaceFittings()[0];auto options=rule.fitting.options();options.fairing.shapePolicy=SurfaceFairingShapePolicy::PreserveMaximumEdgeStrain;rule.fitting=SurfaceFitSettings(options);editor.setSurfaceFittings(std::span(&rule,1));
    const auto current=exportObject3DProject(project.editor);CHECK(current[8]==12);Object3DEngine restored;auto migrated=importObject3DProject(restored.reference(),current);CHECK(migrated.editor.get().surfaceFittings()[0]==rule&&migrated.editor.get().undo());
    CHECK(migrated.editor.get().surfaceFittings()[0].fitting.options().fairing.shapePolicy==SurfaceFairingShapePolicy::LegacyDisplacement&&migrated.editor.get().redo());
    CHECK(migrated.editor.get().surfaceFittings()[0].fitting.options().fairing.shapePolicy==SurfaceFairingShapePolicy::PreserveMaximumEdgeStrain);
}
int main(int argc,char **argv)
{
    try{if(argc<2||argc>4)return 2;const std::string_view mode=argv[1];if(mode=="energy")energy();else if(mode=="pins")pins();else if(mode=="strain")strain();else if(mode=="budgets")budgets();else if(mode=="noop")noop();else if(mode=="validation")validation();else if(mode=="settings")settings();else if(mode=="project"&&argc==4)project(argv[2],false,argv[3]);else if(mode=="legacy_v10"&&argc>=3)project(argv[2],true);else if(mode=="legacy_v11"&&argc>=3)legacyFairing(argv[2]);else return 2;std::cout<<mode<<" passed\n";}
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
