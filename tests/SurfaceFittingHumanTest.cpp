#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#include <iiActorObject/NSCharacter/HumanMorphology.h>
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#include <iiActorObject/MeshCollider.h>
#include <iiActorObject/Object3DInterchange.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
void save(const std::filesystem::path &p,const std::vector<std::uint8_t> &bytes){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());CHECK(f.good());}
std::vector<Vector3> points(const Object3DMesh &mesh){std::vector<Vector3> out;for(const auto &p:mesh.topology->points)out.push_back(p.position);return out;}
std::vector<std::uint32_t> alternateQuads(const Object3DMesh &mesh)
{
    const auto &t=*mesh.topology;const auto &d=t.layout->definition();std::set<std::uint32_t> hidden(d.hiddenPolygons.begin(),d.hiddenPolygons.end());
    const auto cache=t.layout->triangles(),polygons=t.layout->trianglePolygons();std::vector<std::vector<std::uint32_t>> original(d.polygons.size());
    for(std::size_t i=0;i<cache.size();++i)original[polygons[i/3]].push_back(d.layouts[d.renderToLayout[cache[i]]].position);
    std::vector<std::uint32_t> result;
    for(std::size_t f=0;f<d.polygons.size();++f)
    {
        if(hidden.contains(static_cast<std::uint32_t>(f)))continue;const auto &face=d.polygons[f];
        if(face.size()!=4){result.insert(result.end(),original[f].begin(),original[f].end());continue;}
        std::array<std::uint32_t,4> p;for(unsigned i=0;i<4;++i)p[i]=d.layouts[face[i]].position;bool diagonal02=false;
        for(std::size_t i=0;i<original[f].size();i+=3){bool a=false,c=false;for(unsigned k=0;k<3;++k){a|=original[f][i+k]==p[0];c|=original[f][i+k]==p[2];}diagonal02|=a&&c;}
        if(diagonal02)result.insert(result.end(),{p[0],p[1],p[3],p[1],p[2],p[3]});else result.insert(result.end(),{p[0],p[1],p[2],p[0],p[2],p[3]});
    }
    return result;
}
Object3DMesh garment(const Object3DMesh &body,std::size_t material)
{
    auto result=body;result.name="FittedShirt";result.material=material;auto &t=*result.topology;auto d=t.layout->definition();d.sourceFormat="iiActorObject.SurfaceFittingFixture";d.sourceElement=NoTopologyIndex;
    double height=0;std::vector<Vector3> normals(t.points.size());const auto &old=*body.topology;const auto &source=old.layout->definition();
    for(std::size_t i=0;i<source.renderToLayout.size();++i)
    {const auto p=source.layouts[source.renderToLayout[i]].position;const auto n=body.geometry.vertices[i].normal;normals[p].x+=n.x;normals[p].y+=n.y;normals[p].z+=n.z;height=std::max(height,t.points[p].position.y);}
    std::set<std::uint32_t> hidden(source.hiddenPolygons.begin(),source.hiddenPolygons.end());std::vector<bool> selected(source.polygons.size());
    for(std::size_t f=0;f<source.polygons.size();++f)
    {
        if(hidden.contains(static_cast<std::uint32_t>(f)))continue;Vector3 centre;for(auto layout:source.polygons[f]){const auto p=t.points[source.layouts[layout].position].position;centre.x+=p.x;centre.y+=p.y;centre.z+=p.z;}const auto n=source.polygons[f].size();centre={centre.x/n,centre.y/n,centre.z/n};
        selected[f]=centre.y>height*.52&&centre.y<height*.88&&std::abs(centre.x)<height*.24;
    }
    d.polygons.clear();d.polygonGroups.clear();d.polygonMaterials.clear();d.hiddenPolygons.clear();d.renderToLayout.clear();
    const auto triangles=old.layout->triangles(),faces=old.layout->trianglePolygons();std::set<std::uint32_t> visible;
    for(std::size_t i=0;i<triangles.size();i+=3)if(selected[faces[i/3]])
    {
        std::vector<std::uint32_t> face;for(unsigned k=0;k<3;++k){const auto layout=source.renderToLayout[triangles[i+k]];face.push_back(layout);visible.insert(source.layouts[layout].position);}d.polygons.push_back(std::move(face));d.polygonGroups.push_back("shirt");d.polygonMaterials.push_back(static_cast<std::uint32_t>(material));
    }
    CHECK(visible.size()>500&&d.polygons.size()>500);std::uint32_t hem=*visible.begin();double lowest=1e10;
    for(auto p:visible)
    {
        const auto n=std::hypot(normals[p].x,normals[p].y,normals[p].z);CHECK(n>0);normals[p]={normals[p].x/n,normals[p].y/n,normals[p].z/n};
        auto &v=t.points[p].position;v.x-=normals[p].x*.006;v.y-=normals[p].y*.006;v.z-=normals[p].z*.006;if(v.y<lowest){lowest=v.y;hem=p;}
    }
    t.morphTargets.clear();TopologyMorph detail;detail.name="GarmentDetail";detail.deltas={{hem,{normals[hem].x*.0005,normals[hem].y*.0005,normals[hem].z*.0005}}};t.morphTargets.push_back(detail);
    t.layout=MeshTopologyLayout::create(std::move(d),t.points);recomputeTopologyNormals(t);result.geometry={};result.textureCoordinates.clear();result.triangleMaterials.clear();return result;
}
SurfaceFitResult fit(CharacterEditor &editor,const SurfaceFitOptions &options)
{
    auto body=MeshCollider::fromMesh(editor.asset()->data().meshes[0]);const auto p=points(editor.asset()->data().meshes[0]);const auto tri=alternateQuads(editor.asset()->data().meshes[0]);MeshCollider alternate(p,tri);
    CHECK(body.statistics().components==1&&alternate.statistics().components==1);const std::array<const SurfaceCollider*,2> obstacles{&body,&alternate};
    auto result=editor.fitSurface(4,obstacles,options);CHECK(result.converged&&result.pointViolations==0&&result.triangleViolations==0);auto published=editor.asset()->data().meshes[4];
    const auto verify=fitMeshSurface(published,obstacles,options);CHECK(verify.converged&&verify.changedPoints==0);return result;
}
void positions(std::ostream &f,std::span<const Vector3> values)
{f<<'[';bool first=true;for(auto p:values){if(!first)f<<',';first=false;f<<'['<<p.x<<','<<p.y<<','<<p.z<<']';}f<<']';}
void references(const Object3DAsset &asset,const SurfaceFitOptions &options,const SurfaceFitResult &fit,const std::filesystem::path &path)
{
    std::ofstream f(path);f<<std::setprecision(17)<<"{\"clearance\":"<<options.clearance<<",\"tolerance\":"<<options.tolerance<<",\"minimumVertexClearance\":"<<fit.minimumVertexClearance<<",\"minimumTriangleClearance\":"<<fit.minimumTriangleClearance<<",\"meshes\":[";Pose pose(asset.skeleton());
    for(std::size_t i=0;i<asset.data().meshes.size();++i)
    {
        if(i)f<<',';const auto &m=asset.data().meshes[i];const auto &skin=*asset.meshes()[i];f<<"{\"name\":"<<std::quoted(m.name)<<",\"sourcePoints\":";positions(f,points(m));f<<",\"renderPoints\":";
        std::vector<DeformedVertex> deformed(m.geometry.vertices.size());std::vector<double> morphs(m.geometry.morphTargets.size());skin.deform(pose,0,morphs,deformed);std::vector<Vector3> p;for(auto v:deformed)p.push_back(v.position);positions(f,p);f<<",\"morphCount\":"<<m.topology->morphTargets.size()<<'}';
    }
    f<<"]}\n";CHECK(f.good());
}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=6)return 2;Object3DEngine engine;MakeHumanTemplateOptions options;options.macroAssets=argv[2];options.faceAssets=argv[3];options.expressionAssets=argv[4];auto loaded=loadMakeHumanTemplate(engine.reference(),argv[1],options);
        auto definition=loaded.program().get().definition();auto base=definition.base.get().data();const auto blue=base.materials.size();Object3DMaterial material;material.name="PrototypeShirt";material.baseColor={.035,.13,.55,1};material.roughness=.8;base.materials.push_back(material);definition.base=engine.createObject(std::move(base));CharacterTemplate human(engine.createTemplate(std::move(definition)));
        HumanMacroParameters parameters;parameters.masculinity=.6;parameters.muscle=.65;parameters.bodyMass=.4;auto editor=human.edit(makeHumanMacroValues(parameters),1);
        auto clothing=garment(editor.asset()->data().meshes[0],blue);auto data=editor.asset()->data();data.meshes={clothing};auto canonical=engine.createObject(std::move(data));SurfaceAttachmentOptions attachment;attachment.binding.maximumDistance=.04;attachment.skin.maximumDistance=.04;
        const auto attached=editor.attachSurface(canonical.get().data().meshes[0],0,attachment);CHECK(attached.mesh==4&&attached.skin.transferred>500);
        SurfaceFitOptions fitting;fitting.clearance=.002;fitting.tolerance=1e-5;fitting.edgeStiffness=0;fitting.iterations=96;fitting.maximumNodeVisits=fitting.maximumTriangleTests=256'000'000;
        const std::filesystem::path output(argv[5]);std::filesystem::create_directories(output);double maximumBefore=0,maximumAfter=0,minimumClearance=1e10;std::size_t triangleProjections=0,nodeVisits=0,triangleTests=0,changed=0;
        for(const auto label:{"adult","child"})
        {
            if(std::string_view(label)=="child"){parameters.ageYears=10;parameters.muscle=.45;parameters.bodyMass=.6;editor.setTemplateValues(makeHumanMacroValues(parameters));}
            const auto result=fit(editor,fitting);maximumBefore=std::max(maximumBefore,result.maximumViolationBefore);maximumAfter=std::max(maximumAfter,result.maximumViolationAfter);minimumClearance=std::min(minimumClearance,result.minimumTriangleClearance);triangleProjections+=result.triangleProjections;nodeVisits+=result.nodeVisits;triangleTests+=result.triangleTests;changed+=result.changedPoints;
            std::cout<<label<<" fit iterations="<<result.iterations<<" points="<<result.changedPoints<<" triangleProjections="<<result.triangleProjections<<" residual="<<result.maximumViolationAfter<<std::endl;
            auto portable=human.bakeFaceMorphs(*editor.asset());CHECK(portable->data().meshes.size()==5&&portable->data().bones.size()==163&&portable->data().images.size()==3);CHECK(portable->data().meshes[4].topology->morphTargets.size()==60);
            const auto folder=output/label;std::filesystem::create_directories(folder);FbxExportOptions fbxOptions;fbxOptions.allowMorphNormalMetadata=true;
            save(folder/"FittedHuman.glb",exportObject3DGlb(portable->object().get()));save(folder/"FittedHuman.fbx",exportObject3DFbx(portable->object().get(),fbxOptions));references(portable->object().get(),fitting,result,folder/"FittedHumanReference.json");
        }
        auto rig=human.createFaceRig(*editor.asset());const auto bytes=exportProject(editor,std::array{rig});CHECK(bytes[8]==8);save(output/"FittedHuman.ii3dproject",bytes);Object3DEngine restoredEngine;auto restored=importProject(restoredEngine.reference(),bytes);
        CHECK(restored.rigs.size()==1&&restored.editor.undo()&&restored.editor.redo());parameters.ageYears=28;restored.editor.setTemplateValues(makeHumanMacroValues(parameters));const auto restoredFit=fit(restored.editor,fitting);CHECK(restoredFit.converged);
        CharacterTemplate restoredHuman(restored.editor.session().get().templateProgram());auto rebound=restoredHuman.createFaceRig(*restored.editor.asset());CHECK(rebound.get().evaluateNamed(std::array{NamedRigControl{"JawDrop",.5}}).joints.size()==163);
        CHECK(maximumBefore>.001&&maximumAfter<=fitting.tolerance&&minimumClearance>=fitting.clearance-fitting.tolerance&&triangleProjections>0);
        std::cout<<std::setprecision(17)<<"surfaceFitCases=3 sourceBodyPoints=19158 garmentPoints="<<attached.skin.transferred<<" changedPoints="<<changed<<" triangleProjections="<<triangleProjections<<" nodeVisits="<<nodeVisits<<" triangleTests="<<triangleTests<<" beforeViolation="<<maximumBefore<<" afterViolation="<<maximumAfter<<" minimumTriangleClearance="<<minimumClearance<<" projectBytes="<<bytes.size()<<" macroRebuild=1 rigRebind=1\n";
    }
    catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
