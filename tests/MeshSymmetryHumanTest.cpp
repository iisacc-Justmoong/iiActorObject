#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#include <iiActorObject/NSCharacter/HumanMorphology.h>
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DProject.h>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace iiActorObject;
using namespace iiActorObject::NSCharacter;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
double distance(Vector3 a,Vector3 b){return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);}
void save(const std::filesystem::path &p,const std::vector<std::uint8_t> &bytes){std::ofstream file(p,std::ios::binary);file.write(reinterpret_cast<const char *>(bytes.data()),bytes.size());CHECK(file.good());}
const TopologyMorph &morph(const MeshTopology &t,std::string_view name)
{for(const auto &m:t.morphTargets)if(m.name==name)return m;throw std::out_of_range("Expected morph is missing");}
std::vector<Vector3> pointDeltas(const MeshTopology &t,const TopologyMorph &m)
{std::vector<Vector3> values(t.points.size());for(const auto &delta:m.deltas)values.at(delta.point)=delta.position;return values;}
std::vector<Vector3> normalDeltas(const MeshTopology &t,const TopologyMorph &m)
{std::vector<Vector3> values(t.layout->definition().layouts.size());const auto deltas=m.normalDeltas?*m.normalDeltas:topologyMorphNormalDeltas(t,m);for(const auto &delta:deltas)values.at(delta.layout)=delta.normal;return values;}
void positions(std::ostream &file,std::span<const Vector3> points)
{file<<'[';bool first=true;for(auto p:points){if(!first)file<<',';first=false;file<<'['<<p.x<<','<<p.y<<','<<p.z<<']';}file<<']';}
void meshReferences(std::ostream &file,const Object3DAsset &asset,std::string_view name={})
{
    Pose pose(asset.skeleton());file<<'[';
    for(std::size_t i=0;i<asset.data().meshes.size();++i)
    {
        const auto &mesh=asset.data().meshes[i];const auto &t=*mesh.topology;if(i)file<<',';file<<"{\"name\":"<<std::quoted(mesh.name)<<",\"sourcePoints\":";
        std::vector<Vector3> points;for(const auto &p:t.points)points.push_back(p.position);
        for(const auto &m:t.morphTargets)if(m.name==name)for(const auto &d:m.deltas){points[d.point].x+=d.position.x;points[d.point].y+=d.position.y;points[d.point].z+=d.position.z;}
        positions(file,points);file<<",\"renderPoints\":";const auto &skin=*asset.meshes()[i];const auto &lod=skin.lods()[0];std::vector<double> values(lod.morphTargets.size());
        for(std::size_t k=0;k<values.size();++k)if(lod.morphTargets[k].name==name)values[k]=1;std::vector<DeformedVertex> output(lod.vertices.size());skin.deform(pose,0,values,output);points.clear();for(auto p:output)points.push_back(p.position);positions(file,points);
        file<<",\"morphs\":[";for(std::size_t k=0;k<t.morphTargets.size();++k){if(k)file<<',';file<<std::quoted(t.morphTargets[k].name);}file<<"]}";
    }
    file<<']';
}
void reference(const Object3DAsset &asset,const std::filesystem::path &path)
{
    std::ofstream file(path);file<<std::setprecision(17)<<"{\"meshes\":";meshReferences(file,asset);file<<",\"units\":[";
    bool first=true;for(const auto name:{"mirror/LeftUpperLidClosed","mirror/JawDrop","mirror/TongueOut","mirror/MouthLeftPullUp","DetailRight"})
    {if(!first)file<<',';first=false;file<<"{\"name\":"<<std::quoted(name)<<",\"meshes\":";meshReferences(file,asset,name);file<<'}';}file<<"]}\n";CHECK(file.good());
}
void correspondence(const Object3DData &base,std::span<const MeshSymmetryMap> maps,
    std::span<const MorphMirrorRequest> requests,const std::filesystem::path &path)
{
    std::ofstream file(path);file<<std::setprecision(17)<<"{\"meshes\":[";
    for(std::size_t i=0;i<maps.size();++i)
    {
        if(i)file<<',';const auto &d=base.meshes[i].topology->layout->definition();const auto normal=maps[i].plane().normal;
        file<<"{\"name\":"<<std::quoted(base.meshes[i].name)<<",\"normal\":["<<normal.x<<','<<normal.y<<','<<normal.z<<"],\"mirrors\":[";
        for(std::size_t p=0;p<maps[i].mirrors().size();++p){if(p)file<<',';file<<maps[i].mirrors()[p];}file<<"],\"renderToPoint\":[";
        for(std::size_t p=0;p<d.renderToLayout.size();++p){if(p)file<<',';file<<d.layouts[d.renderToLayout[p]].position;}file<<"]}";
    }
    file<<"],\"pairs\":[";for(std::size_t i=0;i<requests.size();++i){if(i)file<<',';file<<"{\"source\":"<<std::quoted(requests[i].source)<<",\"target\":"<<std::quoted(requests[i].target)<<'}';}file<<"]}\n";CHECK(file.good());
}
int main(int argc,char **argv)
{
    try
    {
        if(argc!=6)return 2;Object3DEngine engine;MakeHumanTemplateOptions options;options.macroAssets=argv[2];options.faceAssets=argv[3];options.expressionAssets=argv[4];
        auto human=loadMakeHumanTemplate(engine.reference(),argv[1],options);HumanMacroParameters parameters;parameters.masculinity=.6;parameters.muscle=.65;parameters.bodyMass=.4;
        auto editor=human.edit(makeHumanMacroValues(parameters),1);const auto &base=human.program().get().definition().base.get().data();std::vector<MeshSymmetryMap> maps;std::size_t mapped=0,visits=0;double matchError=0;
        MeshSymmetryOptions matching;matching.maximumDistance=5e-5;matching.centreTolerance=1e-6;
        for(const auto &mesh:base.meshes)
        {
            try{maps.push_back(MeshSymmetryMap::create(mesh,matching));}
            catch(const std::exception &e){throw std::runtime_error(mesh.name+": "+e.what());}
            mapped+=maps.back().mappedPointCount();visits+=maps.back().pointVisits();matchError=std::max(matchError,maps.back().maximumMatchDistance());
        }
        CHECK(maps.size()==4&&mapped==24316);const auto &body=*editor.asset()->data().meshes[0].topology;double maxY=-1e10;std::set<std::uint32_t> visible;
        for(auto layout:body.layout->definition().renderToLayout){const auto p=body.layout->definition().layouts[layout].position;visible.insert(p);maxY=std::max(maxY,body.points[p].position.y);}
        std::uint32_t selected=NoTopologyIndex;double closest=1e10;
        for(auto p:visible)
        {const auto v=body.points[p].position;if(v.x>=-.025||v.y<maxY-.25)continue;const auto d=distance(v,{-.05,maxY-.12,.08});if(d<closest){closest=d;selected=p;}}
        CHECK(selected!=NoTopologyIndex);const auto centre=body.points[selected].position;const auto mask=makeMeshBrushMask(editor.asset()->data().meshes[0],{centre,.025,1});
        TopologyMorph detail;detail.name="DetailLeft";for(std::size_t i=0;i<mask.size();++i)if(mask[i]>0&&maps[0].sides()[i]<0)detail.deltas.push_back({static_cast<std::uint32_t>(i),{.001*mask[i],.0015*mask[i],.003*mask[i]}});CHECK(!detail.deltas.empty());editor.setControlMorph(0,detail);
        CHECK(editor.mirrorMorphs(0,maps[0],std::array{MorphMirrorRequest{"DetailLeft","DetailRight"}}).created==1);
        editor.moveControlPoints(0,std::array{TopologyPointMove{selected,{0,0,.001}}});MeshSymmetryEditOptions direction;direction.direction=MeshSymmetryDirection::NegativeToPositive;const auto shape=editor.symmetrizeMesh(0,maps[0],direction);CHECK(shape.changedPoints>0);
        const auto &points=editor.asset()->data().meshes[0].topology->points;CHECK(distance(points[maps[0].mirrors()[selected]].position,maps[0].reflectPoint(points[selected].position))<1e-12);
        const std::filesystem::path output(argv[5]);std::filesystem::create_directories(output);std::size_t requestsChecked=0;double positionError=0,normalError=0;
        std::vector<MorphMirrorRequest> requests,backwards;for(const auto &unit:human.program().get().definition().poseProgram->units){requests.push_back({object3DPoseMorphName(unit.name),"mirror/"+unit.name});backwards.push_back({"mirror/"+unit.name,"roundtrip/"+unit.name});}CHECK(requests.size()==59);
        correspondence(base,maps,requests,output/"SymmetryCorrespondence.json");
        for(const auto label:{"adult","child"})
        {
            if(std::string_view(label)=="child"){parameters.ageYears=10;editor.setTemplateValues(makeHumanMacroValues(parameters));}
            auto baked=human.bakeFaceMorphs(*editor.asset());auto data=baked->data();
            for(std::size_t i=0;i<data.meshes.size();++i)
            {
                const auto before=*data.meshes[i].topology;const auto result=iiActorObject::mirrorMorphs(data.meshes[i],maps[i],requests);CHECK(result.created==59&&result.unmappedSourceDeltas==0);auto roundtrip=data.meshes[i];CHECK(iiActorObject::mirrorMorphs(roundtrip,maps[i],backwards).created==59);
                for(std::size_t k=0;k<requests.size();++k)
                {
                    const auto source=pointDeltas(before,morph(before,requests[k].source)),mirrored=pointDeltas(*data.meshes[i].topology,morph(*data.meshes[i].topology,requests[k].target));
                    const auto back=pointDeltas(*roundtrip.topology,morph(*roundtrip.topology,backwards[k].target));
                    for(std::size_t p=0;p<source.size();++p){positionError=std::max(positionError,distance(mirrored[maps[i].mirrors()[p]],maps[i].reflectVector(source[p])));positionError=std::max(positionError,distance(source[p],back[p]));}
                    const auto expected=normalDeltas(before,morph(before,requests[k].source)),actual=normalDeltas(*roundtrip.topology,morph(*roundtrip.topology,backwards[k].target));CHECK(expected.size()==actual.size());for(std::size_t l=0;l<expected.size();++l)normalError=std::max(normalError,distance(expected[l],actual[l]));++requestsChecked;
                }
            }
            auto portable=engine.createObject(std::move(data));const auto path=output/label;std::filesystem::create_directories(path);save(path/"SymmetryHuman.glb",exportObject3DGlb(portable.get()));save(path/"SymmetryHuman.fbx",exportObject3DFbx(portable.get()));reference(portable.get(),path/"SymmetryHumanReference.json");
            CHECK(portable.get().data().meshes[0].topology->morphTargets.size()==120&&portable.get().data().images.size()==3);CHECK(morph(*portable.get().data().meshes[0].topology,"DetailRight").deltas.size()==detail.deltas.size());
        }
        CHECK(positionError<2e-12&&normalError<2e-12&&requestsChecked==472);auto rig=human.createFaceRig(*editor.asset());const auto bytes=exportProject(editor,std::array{rig});CHECK(bytes[8]==8);save(output/"SymmetryHuman.ii3dproject",bytes);
        Object3DEngine destination;auto restored=importProject(destination.reference(),bytes);CHECK(restored.rigs.size()==1&&restored.editor.undo()&&restored.editor.redo());CHECK(morph(*restored.editor.asset()->data().meshes[0].topology,"DetailRight").deltas.size()==detail.deltas.size());
        CHECK(restored.rigs[0].get().evaluateNamed(std::array{NamedRigControl{"JawDrop",.5}}).joints.size()==163);
        std::cout<<std::setprecision(17)<<"symmetryPoints="<<mapped<<" pointVisits="<<visits<<" maximumMatchDistance="<<matchError<<" mirroredUnitCases="<<requestsChecked<<" positionError="<<positionError<<" normalRoundtripError="<<normalError<<" detailPoints="<<detail.deltas.size()<<" symmetrizedPoints="<<shape.changedPoints<<" projectBytes="<<bytes.size()<<" macroRebuild=1 rigRestore=1\n";
    }
    catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
