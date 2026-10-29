#include <iiActorObject/Object3DExternalEdits.h>
#include <iiActorObject/Object3DEditor.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DProject.h>
#include <iiActorObject/SkeletonScale.h>
#include <iiActorObject/Features.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterEditor.h>
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
using namespace iiActorObject;
#define CHECK(x) do{if(!(x))throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #x);}while(false)
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}CHECK(caught);}
void near(Vector3 a,Vector3 b){if(std::hypot(a.x-b.x,a.y-b.y,a.z-b.z)>=1e-8)throw std::runtime_error("Vector mismatch ("+std::to_string(a.x)+","+std::to_string(a.y)+","+std::to_string(a.z)+") vs ("+std::to_string(b.x)+","+std::to_string(b.y)+","+std::to_string(b.z)+")");}
void near(const Matrix4 &a,const Matrix4 &b){for(std::size_t i=0;i<16;++i)CHECK(std::abs(a.elements()[i]-b.elements()[i])<1e-8);}
std::vector<Matrix4> world(const Object3DData &d){Pose p(std::make_shared<Skeleton>(d.bones));const auto m=p.worldMatrices();return {m.begin(),m.end()};}
Object3DData data()
{
    auto d=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 7 8 9\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nvn 0 0 1\nf 1/1/1 2/2/1 3/3/1 4/4/1\n");d.kind=Object3DKind::SkeletalMesh;
    d.bones={{"root",NoBone,{{},{},{1.2,1.2,1.2}}},{"hand",2,{{0,1,0}}},{"mount",0,{{1,0,0},{},{.8,.8,.8}}}};
    for(auto &p:d.meshes[0].topology->points)p.weights={{0,.5},{1,.5}};
    TopologyMorph morph;morph.name="Smile";morph.sourceTarget=42;morph.deltas={{2,{0,0,.1}}};d.meshes[0].topology->morphTargets={morph};return d;
}
struct Scene
{
    Object3DEngine engine;
    Object3DHandle source,baseline;
    Object3DExternalIdentity ids;
    Object3DExternalOptions options;
    Scene(){source=engine.createObject(data());auto b=source.get().data();(void)normalizeSkeletonBindScale(b);auto t=b.bones[1].bindTransform;t.rotation=Quaternion::fromAxisAngle({0,1,0},.4);(void)SkeletonEditor::setTransform(b,1,t);baseline=engine.createObject(std::move(b));ids=makeObject3DExternalIdentity(baseline.get().data(),"external-test");options.document=ids.document;}
    Object3DExternalSnapshot base(){return {baseline.get(),ids};}
};
void changes()
{
    Scene s;auto d=s.baseline.get().data();d.meshes[0].topology->points[2].position.x+=.03;d.meshes[0].topology->points[0].weights={{1,1}};d.meshes[0].name="Offline body";
    CHECK(SkeletonEditor::reparent(d,1,0));auto t=d.bones[2].bindTransform;t.position.x+=.02;CHECK(SkeletonEditor::setTransform(d,2,t,JointTransformSpace::Local,true));d.bones[1].name="offline hand";auto edited=s.engine.createObject(std::move(d));auto current=s.source.get().data();const auto before=world(current);
    const auto r=applyObject3DExternalEdits(current,s.source.get(),s.base(),{edited.get(),s.ids},s.options);CHECK(r.positions==1&&r.weightRows==1&&r.parents==1&&r.names==2&&r.changed());
    near(current.meshes[0].topology->points[2].position,{1.03,1,0});CHECK(current.meshes[0].topology->points[0].weights.size()==1&&current.meshes[0].topology->points[0].weights[0].bone==1);CHECK(current.bones[1].parent==0&&current.bones[1].name=="offline hand"&&current.meshes[0].name=="Offline body");
    near(world(current)[1],before[1]);near(world(current)[2].transformPoint({}),Vector3{before[2].elements()[12]+.02,before[2].elements()[13],before[2].elements()[14]});CHECK(current.meshes[0].topology->layout==s.source.get().data().meshes[0].topology->layout);CHECK(current.meshes[0].topology->points[4].position==Vector3(7,8,9));
}
void noop()
{
    Scene s;auto e=s.engine.editObject(s.source);const auto id=e.get().object().id(),rev=e.get().revision();const auto r=e.get().applyExternalEdits(s.source.get(),s.base(),s.base(),s.options);CHECK(!r.changed()&&e.get().revision()==rev&&e.get().object().id()==id);
}
void identity()
{
    Scene s;auto current=s.source.get().data();auto bad=s.ids;bad.meshes[0].points[1]=bad.meshes[0].points[0];rejects([&]{(void)applyObject3DExternalEdits(current,s.source.get(),s.base(),{s.baseline.get(),bad},s.options);});
    bad=s.ids;bad.document="other";rejects([&]{(void)applyObject3DExternalEdits(current,s.source.get(),s.base(),{s.baseline.get(),bad},s.options);});bad=s.ids;bad.bones[1].source=0;rejects([&]{(void)applyObject3DExternalEdits(current,s.source.get(),s.base(),{s.baseline.get(),bad},s.options);});near(current.meshes[0].topology->points[2].position,{1,1,0});
}
void conflicts()
{
    Scene s;auto d=s.baseline.get().data();d.meshes[0].topology->points[0].weights={{1,1}};auto edited=s.engine.createObject(d);auto current=s.source.get().data();current.meshes[0].topology->points[0].weights={{0,1}};
    rejects([&]{(void)applyObject3DExternalEdits(current,s.source.get(),s.base(),{edited.get(),s.ids},s.options);});CHECK(current.meshes[0].topology->points[0].weights[0].bone==0);
    s.options.overwriteConflicts=true;CHECK(applyObject3DExternalEdits(current,s.source.get(),s.base(),{edited.get(),s.ids},s.options).weightRows==1);CHECK(current.meshes[0].topology->points[0].weights[0].bone==1);
}
void attributes()
{
    Scene s;auto d=s.baseline.get().data();d.meshes[0].topology->textureCoordinates[2].u+=.1;d.meshes[0].topology->morphTargets[0].deltas[0].position.z+=.05;d.materials[0].roughness-=.1;auto edited=s.engine.createObject(d);auto current=s.source.get().data();
    const auto r=applyObject3DExternalEdits(current,s.source.get(),s.base(),{edited.get(),s.ids},s.options);CHECK(r.uvs==1&&r.morphs==1&&r.materials==1);CHECK(std::abs(current.meshes[0].topology->textureCoordinates[2].u-1.1)<1e-10);CHECK(current.meshes[0].topology->morphTargets[0].sourceTarget==42&&std::abs(current.meshes[0].topology->morphTargets[0].deltas[0].position.z-.15)<1e-10);
}
void materialSurface()
{
    Scene s;auto changed=s.baseline.get().data();changed.materials[0].surface={Object3DAlphaMode::Mask,.25,true};
    const auto edited=s.engine.createObject(std::move(changed));auto current=s.source.get().data();
    const auto result=applyObject3DExternalEdits(current,s.source.get(),s.base(),{edited.get(),s.ids},s.options);
    CHECK(result.materials==1&&current.materials[0].surface==Object3DMaterialSurface({Object3DAlphaMode::Mask,.25,true}));
    auto conflict=s.source.get().data();conflict.materials[0].surface={Object3DAlphaMode::Blend,.5,false};
    rejects([&]{(void)applyObject3DExternalEdits(conflict,s.source.get(),s.base(),{edited.get(),s.ids},s.options);});
    CHECK(conflict.materials[0].surface.alphaMode==Object3DAlphaMode::Blend);
    s.options.overwriteConflicts=true;
    CHECK(applyObject3DExternalEdits(conflict,s.source.get(),s.base(),{edited.get(),s.ids},s.options).materials==1);
    CHECK(conflict.materials[0].surface==Object3DMaterialSurface({Object3DAlphaMode::Mask,.25,true}));
    s.options.overwriteConflicts=false;auto cutoffConflict=s.source.get().data();cutoffConflict.materials[0].surface.alphaCutoff=.75;
    rejects([&]{(void)applyObject3DExternalEdits(cutoffConflict,s.source.get(),s.base(),{edited.get(),s.ids},s.options);});
    CHECK(cutoffConflict.materials[0].surface.alphaCutoff==.75);
}
void layers()
{
    Scene s;Object3DTemplateDefinition program;program.base=s.source;program.targets={{"height",0,{{2,{0,.2,0}}}}};auto e=s.engine.editTemplate(s.engine.createTemplate(program));const auto exported=e.get().object();auto editedData=s.baseline.get().data();editedData.meshes[0].topology->points[2].position.x+=.03;auto edited=s.engine.createObject(editedData);
    e.get().setTemplateValues(std::array{TemplateValue{"height",.5}});CHECK(e.get().applyExternalEdits(exported.get(),s.base(),{edited.get(),s.ids},s.options).positions==1);near(e.get().object().get().data().meshes[0].topology->points[2].position,{1.03,1.1,0});CHECK(e.get().layers().size()==1);
    Object3DEngine destination;auto restored=importObject3DProject(destination.reference(),exportObject3DProject(e));CHECK(restored.editor.get().undo());CHECK(restored.editor.get().redo());restored.editor.get().setTemplateValues(std::array{TemplateValue{"height",1}});near(restored.editor.get().object().get().data().meshes[0].topology->points[2].position,{1.03,1.2,0});
}
void surfaces()
{
    Scene s;Object3DTemplateDefinition definition;auto source=s.source.get().data();auto garment=source.meshes[0];garment.name="Garment";for(auto &p:garment.topology->points)p.position.z+=.02;source.meshes.push_back(garment);definition.surfaces={bindMeshSurface(source,0,1)};auto reference=source;
    source.meshes[0].topology->points[2].position.z+=.03;source.meshes[1].topology->points[2].position.x+=.01;const auto expected=source;unapplyMeshSurfaceBindings(source,reference,definition.surfaces);applyMeshSurfaceBindings(source,reference,definition.surfaces);for(std::size_t m=0;m<source.meshes.size();++m)for(std::size_t p=0;p<source.meshes[m].topology->points.size();++p)near(source.meshes[m].topology->points[p].position,expected.meshes[m].topology->points[p].position);
    definition.base=s.engine.createObject(reference);definition.targets={{"height",0,{{2,{0,.2,0}}}}};auto e=s.engine.editTemplate(s.engine.createTemplate(definition));auto canonical=e.get().object();auto unit=canonical.get().data();(void)normalizeSkeletonBindScale(unit);auto baseline=s.engine.createObject(unit);auto ids=makeObject3DExternalIdentity(baseline.get().data(),s.ids.document);unit.meshes[0].topology->points[2].position.z+=.03;unit.meshes[1].topology->points[2].position.x+=.01;auto edited=s.engine.createObject(unit);
    auto result=e.get().applyExternalEdits(canonical.get(),{baseline.get(),ids},{edited.get(),ids},s.options);CHECK(result.positions==2);near(e.get().object().get().data().meshes[1].topology->points[2].position,{1.01,1,.02});CHECK(e.get().undo()&&e.get().redo());
}
void fbxIdentity()
{
    Scene s;FbxExportOptions exportOptions;exportOptions.externalDocument=s.ids.document;auto bytes=exportObject3DFbx(s.source.get(),exportOptions);FbxImportOptions importOptions;importOptions.restoreAuthoringMetadata=false;auto imported=importObject3DFbx(s.engine.reference(),bytes,importOptions);
    CHECK(imported.externalIdentity.document==s.ids.document&&imported.externalIdentity.bones.size()==3&&imported.externalIdentity.meshes.size()==1&&imported.externalIdentity.materials.size()==1);CHECK(imported.externalIdentity.meshes[0].points.size()==5&&imported.externalIdentity.meshes[0].polygons.size()==1&&imported.externalIdentity.meshes[0].corners.size()==4&&imported.externalIdentity.meshes[0].morphs.size()==1);
}
void glbTriangles()
{
    Object3DEngine engine;auto original=data();
    original.bones={{"root",NoBone,{}},{"hand",0,{{0,1,0}}}};
    auto &topology=*original.meshes[0].topology;
    topology.points.push_back({{8,8,9},{{0,.5},{1,.5}}});
    topology.points.push_back({{7,9,9},{{0,.5},{1,.5}}});
    auto sourceLayout=topology.layout->definition();sourceLayout.positionCount=static_cast<std::uint32_t>(topology.points.size());
    sourceLayout.layouts.insert(sourceLayout.layouts.end(),{{4,0,0},{5,1,0},{6,2,0}});
    sourceLayout.polygons.push_back({4,5,6});sourceLayout.polygonGroups.push_back("hidden");sourceLayout.hiddenPolygons={1};
    topology.layout=MeshTopologyLayout::create(std::move(sourceLayout),topology.points);
    const auto source=engine.createObject(original);
    const auto baseline=importObject3DGlb(engine.reference(),exportObject3DGlb(source.get()));
    const auto &sourceTopology=*source.get().data().meshes[0].topology;
    const auto &sourceDefinition=sourceTopology.layout->definition();
    const auto &baseTopology=*baseline.object.get().data().meshes[0].topology;
    CHECK(baseTopology.points.size()==sourceTopology.points.size());
    CHECK(sourceDefinition.polygons[0].size()==4&&sourceDefinition.hiddenPolygons.size()==1);
    auto ids=makeObject3DExternalIdentity(baseline.object.get().data(),"glb-triangle-edit");
    CHECK(ids.meshes.size()==1&&ids.bones.size()==2&&ids.materials.size()==1);
    for(std::size_t i=0;i<ids.bones.size();++i)ids.bones[i]={baseline.sourceJointToBone.at(i),i};
    auto &meshId=ids.meshes[0];meshId.topology=ExternalMeshTopology::RenderTriangles;
    meshId.polygons.clear();meshId.corners.clear();
    const auto triangles=sourceTopology.layout->triangles();
    CHECK(triangles.size()==6&&baseTopology.layout->definition().polygons.size()==2);
    for(std::size_t i=0;i<triangles.size();i+=3)
    {
        meshId.polygons.push_back(static_cast<std::uint32_t>(i/3));
        for(std::size_t j=0;j<3;++j)
        {
            const auto layout=sourceDefinition.renderToLayout.at(triangles[i+j]);
            meshId.corners.push_back(layout);
            const auto foreign=baseTopology.layout->definition().polygons[i/3][j];
            CHECK(baseTopology.layout->definition().layouts[foreign].position==sourceDefinition.layouts[layout].position);
        }
    }
    auto changed=baseline.object.get().data();auto &foreign=*changed.meshes[0].topology;
    foreign.points[2].position.x+=.03;foreign.points[0].weights={{baseline.sourceJointToBone.at(1),1}};
    const auto &baseDefinition=foreign.layout->definition();
    const auto uvCorner=std::find(meshId.corners.begin(),meshId.corners.end(),2u)-meshId.corners.begin();
    CHECK(uvCorner<meshId.corners.size());
    const auto uvLayout=baseDefinition.polygons[uvCorner/3][uvCorner%3];
    foreign.textureCoordinates[baseDefinition.layouts[uvLayout].textureCoordinate].u+=.1;
    const auto delta=std::find_if(foreign.morphTargets[0].deltas.begin(),foreign.morphTargets[0].deltas.end(),
        [](const auto &value){return value.point==2;});
    CHECK(delta!=foreign.morphTargets[0].deltas.end());delta->position.z+=.05;
    changed.materials[0].roughness-=.1;
    changed.bones[baseline.sourceJointToBone.at(1)].bindTransform.position.x+=.02;
    for(auto &normal:foreign.normals)normal={.6,0,.8};
    auto reorderedLayout=foreign.layout->definition();std::swap(reorderedLayout.polygons[0],reorderedLayout.polygons[1]);
    if(!reorderedLayout.polygonMaterials.empty())std::swap(reorderedLayout.polygonMaterials[0],reorderedLayout.polygonMaterials[1]);
    foreign.layout=MeshTopologyLayout::create(std::move(reorderedLayout),foreign.points);
    const auto edited=engine.createObject(std::move(changed));auto editedIds=ids;
    std::swap(editedIds.meshes[0].polygons[0],editedIds.meshes[0].polygons[1]);
    for(std::size_t i=0;i<3;++i)std::swap(editedIds.meshes[0].corners[i],editedIds.meshes[0].corners[i+3]);
    Object3DTemplateDefinition program;program.base=source;program.targets={{"height",0,{{2,{0,.2,0}}}}};
    auto editor=engine.editTemplate(engine.createTemplate(program));editor.get().setTemplateValues(std::array{TemplateValue{"height",.5}});
    Object3DExternalOptions options;options.document=ids.document;
    const auto report=editor.get().applyExternalEdits(source.get(),{baseline.object.get(),ids},{edited.get(),editedIds},options);
    CHECK(report.positions==1&&report.weightRows==1&&report.uvs==1&&report.morphs==1&&report.materials==1);
    CHECK(report.jointTransforms>=1&&report.normals==1&&report.layer.has_value());
    const auto &result=editor.get().object().get().data();const auto &resultTopology=*result.meshes[0].topology;
    CHECK(std::abs(resultTopology.points[2].position.x-1.03)<1e-6&&std::abs(resultTopology.points[2].position.y-1.1)<1e-6);
    CHECK(resultTopology.points[0].weights.size()==1&&resultTopology.points[0].weights[0].bone==1);
    CHECK(resultTopology.layout->definition()==sourceDefinition&&resultTopology.points[4].position==Vector3(7,8,9));
    CHECK(resultTopology.morphTargets[0].sourceTarget==42&&std::abs(resultTopology.morphTargets[0].deltas[0].position.z-.15)<1e-6);
    CHECK(std::abs(resultTopology.textureCoordinates[2].u-1.1)<1e-6&&std::abs(result.materials[0].roughness-(original.materials[0].roughness-.1))<1e-6);
    CHECK(editor.get().undo()&&editor.get().redo());
#if IIACTOROBJECT_HAS_NSCHARACTER
    auto characterSession=engine.editTemplate(editor.get().templateProgram());
    NSCharacter::CharacterEditor character(characterSession);NSCharacter::CharacterAsset characterSource(source);
    character.setTemplateValues(std::array{TemplateValue{"height",.5}});
    const auto characterReport=character.applyExternalEdits(characterSource,
        {baseline.object.get(),ids},{edited.get(),editedIds},options);
    CHECK(characterReport.positions==1&&characterReport.weightRows==1&&characterReport.layer.has_value());
    CHECK(&character.asset()->data()==&characterSession.get().object().get().data());
    CHECK(character.undo()&&character.redo());
#endif
    const auto revision=editor.get().revision();auto wrong=editedIds;wrong.meshes[0].polygons[0]=wrong.meshes[0].polygons[1];
    rejects([&]{(void)editor.get().applyExternalEdits(source.get(),{baseline.object.get(),ids},{edited.get(),wrong},options);});
    CHECK(editor.get().revision()==revision);
    wrong=editedIds;wrong.meshes[0].corners[0]=sourceDefinition.layouts.size();
    rejects([&]{(void)editor.get().applyExternalEdits(source.get(),{baseline.object.get(),ids},{edited.get(),wrong},options);});
    wrong=editedIds;wrong.meshes[0].topology=static_cast<ExternalMeshTopology>(255);
    rejects([&]{(void)editor.get().applyExternalEdits(source.get(),{baseline.object.get(),ids},{edited.get(),wrong},options);});
    wrong=editedIds;wrong.meshes[0].topology=ExternalMeshTopology::SourcePolygons;
    rejects([&]{(void)editor.get().applyExternalEdits(source.get(),{baseline.object.get(),ids},{edited.get(),wrong},options);});
    auto incompleteData=baseline.object.get().data();auto &incompleteTopology=*incompleteData.meshes[0].topology;
    auto incompleteLayout=incompleteTopology.layout->definition();incompleteLayout.polygons.pop_back();
    if(!incompleteLayout.polygonMaterials.empty())incompleteLayout.polygonMaterials.pop_back();
    incompleteLayout.renderToLayout.clear();
    incompleteTopology.layout=MeshTopologyLayout::create(std::move(incompleteLayout),incompleteTopology.points);
    const auto incomplete=engine.createObject(std::move(incompleteData));auto incompleteIds=ids;
    incompleteIds.meshes[0].polygons.pop_back();incompleteIds.meshes[0].corners.resize(3);
    rejects([&]{(void)editor.get().applyExternalEdits(source.get(),{incomplete.get(),incompleteIds},{incomplete.get(),incompleteIds},options);});
    CHECK(editor.get().revision()==revision);
}
void glbTriangleSeams()
{
    Object3DEngine engine;auto input=parseObject3DObj(
        "v 0 0 0\nv 1 0 0\nv 2 0 0\nv 0 1 0\nv 1 1 0\nv 2 1 0\n"
        "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
        "vn 0 0 1\nf 1/1/1 2/2/1 5/3/1 4/4/1\nf 2/5/1 3/6/1 6/7/1 5/8/1\n");
    input.kind=Object3DKind::SkeletalMesh;input.bones={{"root",NoBone,{}}};
    input.materials.push_back({"Second",{.2,.4,.6,1}});auto &original=*input.meshes[0].topology;
    for(auto &point:original.points)point.weights={{0,1}};
    auto sourceLayout=original.layout->definition();sourceLayout.polygonMaterials={0,1};
    original.layout=MeshTopologyLayout::create(std::move(sourceLayout),original.points);
    const auto source=engine.createObject(std::move(input));
    const auto baseline=importObject3DGlb(engine.reference(),exportObject3DGlb(source.get()));
    const auto &sourceTopology=*source.get().data().meshes[0].topology;
    const auto &sourceDefinition=sourceTopology.layout->definition();
    const auto &baseTopology=*baseline.object.get().data().meshes[0].topology;
    CHECK(sourceDefinition.layouts[1].position==sourceDefinition.layouts[4].position);
    CHECK(sourceDefinition.layouts[1].textureCoordinate!=sourceDefinition.layouts[4].textureCoordinate);
    CHECK(baseTopology.points.size()==sourceTopology.points.size());
    auto ids=makeObject3DExternalIdentity(baseline.object.get().data(),"glb-seams");
    CHECK(ids.materials.size()==2&&ids.meshes.size()==1&&ids.bones.size()==1);
    ids.bones[0]={baseline.sourceJointToBone.at(0),0};
    auto &identity=ids.meshes[0];identity.topology=ExternalMeshTopology::RenderTriangles;
    identity.polygons.clear();identity.corners.clear();const auto triangles=sourceTopology.layout->triangles();
    CHECK(triangles.size()==12&&baseTopology.layout->definition().polygons.size()==4);
    for(std::size_t i=0;i<triangles.size();i+=3)
    {
        identity.polygons.push_back(static_cast<std::uint32_t>(i/3));
        for(std::size_t j=0;j<3;++j)
        {
            const auto layout=sourceDefinition.renderToLayout.at(triangles[i+j]);identity.corners.push_back(layout);
            const auto foreign=baseTopology.layout->definition().polygons[i/3][j];
            CHECK(baseTopology.layout->definition().layouts[foreign].position==sourceDefinition.layouts[layout].position);
        }
    }
    auto changed=baseline.object.get().data();auto &foreign=*changed.meshes[0].topology;
    foreign.points[1].position.x+=.05;
    const auto corner=std::find(identity.corners.begin(),identity.corners.end(),1u)-identity.corners.begin();
    CHECK(corner<identity.corners.size());const auto &definition=foreign.layout->definition();
    const auto layout=definition.polygons[corner/3][corner%3];
    foreign.textureCoordinates[definition.layouts[layout].textureCoordinate].u+=.2;
    const auto edited=engine.createObject(std::move(changed));auto current=source.get().data();
    Object3DExternalOptions options;options.document=ids.document;
    const auto report=applyObject3DExternalEdits(current,source.get(),{baseline.object.get(),ids},{edited.get(),ids},options);
    CHECK(report.positions==1&&report.uvs==1);
    const auto &result=*current.meshes[0].topology;
    CHECK(std::abs(result.points[1].position.x-1.05)<1e-6&&result.layout->definition()==sourceDefinition);
    CHECK(std::abs(result.textureCoordinates[sourceDefinition.layouts[1].textureCoordinate].u-1.2)<1e-6);
    CHECK(result.textureCoordinates[sourceDefinition.layouts[4].textureCoordinate]==sourceTopology.textureCoordinates[sourceDefinition.layouts[4].textureCoordinate]);

    GlbExportOptions exportOptions;exportOptions.externalDocument="glb-seams-auto";
    const auto autoBaseline=importObject3DGlb(engine.reference(),exportObject3DGlb(source.get(),exportOptions));
    const auto autoBaseIds=makeObject3DGlbExternalIdentity(source.get(),autoBaseline,exportOptions.externalDocument);
    auto sourceEdit=source.get().data();sourceEdit.meshes[0].topology->points[1].position.x+=.05;
    sourceEdit.meshes[0].topology->textureCoordinates[sourceDefinition.layouts[1].textureCoordinate].u+=.2;
    const auto autoEditedSource=engine.createObject(std::move(sourceEdit));
    const auto autoEdited=importObject3DGlb(engine.reference(),exportObject3DGlb(autoEditedSource.get(),exportOptions));
    const auto autoEditedIds=makeObject3DGlbExternalIdentity(source.get(),autoEdited,exportOptions.externalDocument);
    auto autoCurrent=source.get().data();Object3DExternalOptions autoOptions;autoOptions.document=exportOptions.externalDocument;
    const auto autoReport=applyObject3DExternalEdits(autoCurrent,source.get(),
        {autoBaseline.object.get(),autoBaseIds},{autoEdited.object.get(),autoEditedIds},autoOptions);
    CHECK(autoReport.positions==1&&autoReport.uvs==1);
    CHECK(autoCurrent.meshes[0].topology->textureCoordinates[sourceDefinition.layouts[4].textureCoordinate]==
          sourceTopology.textureCoordinates[sourceDefinition.layouts[4].textureCoordinate]);

    auto one=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    one.kind=Object3DKind::SkeletalMesh;one.bones={{"root",NoBone,{}}};
    for(auto &point:one.meshes[0].topology->points)point.weights={{0,1}};
    const auto single=engine.createObject(std::move(one));const auto polygonIds=makeObject3DExternalIdentity(single.get().data(),"mixed-topology");
    auto triangleIds=polygonIds;triangleIds.meshes[0].topology=ExternalMeshTopology::RenderTriangles;
    auto unchanged=single.get().data();Object3DExternalOptions mismatch;mismatch.document=polygonIds.document;
    rejects([&]{(void)applyObject3DExternalEdits(unchanged,single.get(),{single.get(),polygonIds},{single.get(),triangleIds},mismatch);});
    CHECK(unchanged.meshes[0].topology->points[0].position==single.get().data().meshes[0].topology->points[0].position);
}
Object3DData automaticGlbSource()
{
    auto input=data();input.bones={{"root",NoBone,{}},{"hand",0,{{0,1,0}}},{"finger",1,{{0,.2,0}}}};
    input.meshes[0].topology->points[3].weights.clear();input.meshes[0].topology->points[4].weights={{2,1}};return input;
}
Object3DData automaticGlbMultiSource()
{
    auto input=automaticGlbSource();auto garment=input.meshes[0];garment.name="Garment";garment.material=1;
    for(auto &point:garment.topology->points)point.position.z+=.2;
    input.materials.push_back({"Garment material",{.2,.4,.6,1}});input.meshes.push_back(std::move(garment));return input;
}
void glbAutomaticIdentity()
{
    Object3DEngine engine;auto input=automaticGlbSource();
    const auto source=engine.createObject(std::move(input));GlbExportOptions exportOptions;
    exportOptions.externalDocument="automatic-glb-edit";
    const auto bytes=exportObject3DGlb(source.get(),exportOptions);
    const auto baseline=importObject3DGlb(engine.reference(),bytes);
    const auto baseIds=makeObject3DGlbExternalIdentity(source.get(),baseline,exportOptions.externalDocument);
    CHECK(baseIds.document==exportOptions.externalDocument&&baseIds.meshes.size()==1);
    CHECK(baseIds.meshes[0].topology==ExternalMeshTopology::RenderTriangles&&baseIds.meshes[0].polygons.size()==2);
    auto data=source.get().data();data.meshes[0].topology->points[2].position.x+=.03;
    data.meshes[0].topology->points[0].weights={{1,1}};data.meshes[0].topology->textureCoordinates[2].u+=.1;
    data.meshes[0].topology->morphTargets[0].deltas[0].position.z+=.05;
    data.materials[0].roughness-=.1;data.bones[1].bindTransform.position.x+=.02;
    data.meshes[0].name="Renamed mesh";data.materials[0].name="Renamed material";
    const auto editedSource=engine.createObject(std::move(data));
    const auto edited=importObject3DGlb(engine.reference(),exportObject3DGlb(editedSource.get(),exportOptions));
    const auto editedIds=makeObject3DGlbExternalIdentity(source.get(),edited,exportOptions.externalDocument);
    Object3DTemplateDefinition program;program.base=source;program.targets={{"height",0,{{2,{0,.2,0}}}}};
    auto editor=engine.editTemplate(engine.createTemplate(program));
    editor.get().setTemplateValues(std::array{TemplateValue{"height",.5}});
    Object3DExternalOptions options;options.document=exportOptions.externalDocument;
    const auto report=editor.get().applyExternalEdits(source.get(),{baseline.object.get(),baseIds},{edited.object.get(),editedIds},options);
    CHECK(report.positions==1&&report.weightRows==1&&report.uvs==1&&report.morphs==1&&report.materials==1&&report.jointTransforms>=1&&report.names==2);
    const auto &result=*editor.get().object().get().data().meshes[0].topology;
    CHECK(std::abs(result.points[2].position.x-1.03)<1e-6&&std::abs(result.points[2].position.y-1.1)<1e-6);
    CHECK(result.layout->definition()==source.get().data().meshes[0].topology->layout->definition());
    CHECK(result.points[4].position==Vector3(7,8,9)&&result.morphTargets[0].sourceTarget==42);
    CHECK(editor.get().undo()&&editor.get().redo());
    rejects([&]{(void)makeObject3DGlbExternalIdentity(source.get(),baseline,"other-document");});
    const auto ordinary=importObject3DGlb(engine.reference(),exportObject3DGlb(source.get()));
    rejects([&]{(void)makeObject3DGlbExternalIdentity(source.get(),ordinary,exportOptions.externalDocument);});
    auto incomplete=baseline;CHECK(incomplete.externalIdentity.has_value());
    incomplete.externalIdentity->meshes[0].pointsRestored=false;
    rejects([&]{(void)makeObject3DGlbExternalIdentity(source.get(),incomplete,exportOptions.externalDocument);});
    auto incorrectRoot=baseline;incorrectRoot.externalIdentity->syntheticRoot=0;
    rejects([&]{(void)makeObject3DGlbExternalIdentity(source.get(),incorrectRoot,exportOptions.externalDocument);});
    GlbExportOptions invalid;invalid.externalDocument=std::string(129,'a');
    rejects([&]{(void)exportObject3DGlb(source.get(),invalid);});
    invalid.externalDocument=std::string("bad\0token",9);
    rejects([&]{(void)exportObject3DGlb(source.get(),invalid);});
    invalid.externalDocument=std::string("\xc0\x80",2);
    rejects([&]{(void)exportObject3DGlb(source.get(),invalid);});
    auto renderOnly=source.get().data();renderOnly.meshes[0].topology.reset();
    const auto renderSource=engine.createObject(std::move(renderOnly));
    rejects([&]{(void)exportObject3DGlb(renderSource.get(),exportOptions);});
    auto duplicate=source.get().data();auto &duplicateTopology=*duplicate.meshes[0].topology;
    auto definition=duplicateTopology.layout->definition();definition.polygons.push_back(definition.polygons[0]);
    if(!definition.polygonGroups.empty())definition.polygonGroups.push_back(definition.polygonGroups[0]);
    if(!definition.polygonMaterials.empty())definition.polygonMaterials.push_back(definition.polygonMaterials[0]);
    duplicateTopology.layout=MeshTopologyLayout::create(std::move(definition),duplicateTopology.points);
    const auto duplicateSource=engine.createObject(std::move(duplicate));
    const auto duplicateGlb=importObject3DGlb(engine.reference(),exportObject3DGlb(duplicateSource.get(),exportOptions));
    rejects([&]{(void)makeObject3DGlbExternalIdentity(duplicateSource.get(),duplicateGlb,exportOptions.externalDocument);});
    auto unused=source.get().data();unused.materials.push_back({"Unused material",{.5,.5,.5,1}});
    const auto unusedSource=engine.createObject(std::move(unused));
    const auto unusedGlb=importObject3DGlb(engine.reference(),exportObject3DGlb(unusedSource.get(),exportOptions));
    rejects([&]{(void)makeObject3DGlbExternalIdentity(unusedSource.get(),unusedGlb,exportOptions.externalDocument);});

    auto staticInput=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n");
    const auto staticSource=engine.createObject(std::move(staticInput));CHECK(!staticSource.get().hasRig());
    const auto staticBaseline=importObject3DGlb(engine.reference(),exportObject3DGlb(staticSource.get(),exportOptions));
    const auto staticBaseIds=makeObject3DGlbExternalIdentity(staticSource.get(),staticBaseline,exportOptions.externalDocument);
    auto staticEdit=staticSource.get().data();staticEdit.meshes[0].topology->points[2].position.z+=.02;
    const auto staticEditedSource=engine.createObject(std::move(staticEdit));
    const auto staticEdited=importObject3DGlb(engine.reference(),exportObject3DGlb(staticEditedSource.get(),exportOptions));
    const auto staticEditedIds=makeObject3DGlbExternalIdentity(staticSource.get(),staticEdited,exportOptions.externalDocument);
    auto staticResult=staticSource.get().data();Object3DExternalOptions staticOptions;staticOptions.document=exportOptions.externalDocument;
    const auto staticReport=applyObject3DExternalEdits(staticResult,staticSource.get(),
        {staticBaseline.object.get(),staticBaseIds},{staticEdited.object.get(),staticEditedIds},staticOptions);
    CHECK(staticReport.positions==1&&std::abs(staticResult.meshes[0].topology->points[2].position.z-.02)<1e-6);
#if IIACTOROBJECT_HAS_NSCHARACTER
    NSCharacter::CharacterAsset characterSource(source);
    const auto characterBytes=NSCharacter::exportGlb(characterSource,exportOptions);
    const auto characterImport=NSCharacter::importGlb(engine.reference(),characterBytes);
    const auto characterIds=NSCharacter::makeGlbExternalIdentity(characterSource,characterImport,exportOptions.externalDocument);
    CHECK(characterIds.meshes.size()==1&&characterIds.meshes[0].polygons.size()==2);
#endif
}
void emitAutomaticGlb(bool multi)
{
    Object3DEngine engine;const auto source=engine.createObject(multi?automaticGlbMultiSource():automaticGlbSource());GlbExportOptions options;
    options.externalDocument="automatic-glb-edit";
    for(const auto byte:exportObject3DGlb(source.get(),options))
        std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(byte);
    std::cout<<'\n';
}
void inspectAutomaticGlb(const std::filesystem::path &path,std::string_view mode)
{
    std::ifstream file(path,std::ios::binary);CHECK(file.good());
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
    Object3DEngine engine;const auto source=engine.createObject(mode=="inspect_glb_multi"?automaticGlbMultiSource():automaticGlbSource());
    GlbImportResult imported;
    try{imported=importObject3DGlb(engine.reference(),bytes);}
    catch(const std::exception&){if(mode=="inspect_glb_reject_file")return;throw;}
    if(mode=="inspect_glb_reject"||mode=="inspect_glb_reject_file")
    {
        rejects([&]{(void)makeObject3DGlbExternalIdentity(source.get(),imported,"automatic-glb-edit");});
        return;
    }
    const auto ids=makeObject3DGlbExternalIdentity(source.get(),imported,"automatic-glb-edit");
    if(mode=="inspect_glb_accept")
    {
        const auto &points=imported.object.get().data().meshes[0].topology->points;
        const auto finger=std::find_if(ids.bones.begin(),ids.bones.end(),[](const auto &bone){return bone.source==2;});
        CHECK(finger!=ids.bones.end()&&points[3].weights.empty());
        CHECK(points[4].weights.size()==1&&points[4].weights[0].bone==finger->bone&&points[4].weights[0].weight==1);
    }
    const auto reference=importObject3DGlb(engine.reference(),exportObject3DGlb(source.get(),GlbExportOptions{"automatic-glb-edit"}));
    const auto baseline=makeObject3DGlbExternalIdentity(source.get(),reference,"automatic-glb-edit");
    auto current=source.get().data();Object3DExternalOptions options;options.document="automatic-glb-edit";
    const auto report=applyObject3DExternalEdits(current,source.get(),{reference.object.get(),baseline},{imported.object.get(),ids},options);
    if(mode=="inspect_glb_edit")
    {
        CHECK(report.positions==1&&std::abs(current.meshes[0].topology->points[2].position.x-1.03)<1e-6);
    }
    else if(mode=="inspect_glb_rename")CHECK(report.names==1);
    else CHECK(!report.changed());
}
void groupedMorphs()
{
    Object3DEngine engine;auto d=data();auto &records=d.meshes[0].topology->morphTargets;
    records[0].normalDeltas=std::vector<TopologyNormalDelta>{{0,{.1,0,0}}};
    TopologyMorph blink;blink.name="Blink";blink.sourceTarget=7;blink.deltas={{0,{.01,0,0}}};
    TopologyMorph second;second.name="Smile";second.sourceTarget=9;second.deltas={{2,{0,0,.05}}};
    second.normalDeltas=std::vector<TopologyNormalDelta>{{0,{.2,0,0}}};
    records.push_back(blink);records.push_back(second);auto source=engine.createObject(d);
    const auto identity=makeObject3DExternalIdentity(source.get().data(),"group-test");
    CHECK(identity.meshes[0].morphs.size()==2);
    FbxExportOptions exportOptions;exportOptions.externalDocument="group-test";
    const auto bytes=exportObject3DFbx(source.get(),exportOptions);FbxImportOptions options;options.restoreAuthoringMetadata=false;
    auto baseline=importObject3DFbx(engine.reference(),bytes,options);
    const auto &targets=baseline.object.get().data().meshes[0].topology->morphTargets;
    CHECK(targets.size()==2 && targets[0].name=="Smile" && targets[1].name=="Blink");
    CHECK(baseline.externalIdentity.meshes[0].morphs.size()==2);
    const auto point=std::find_if(targets[0].deltas.begin(),targets[0].deltas.end(),[](const auto &v){return v.point==2;});
    CHECK(point!=targets[0].deltas.end() && std::abs(point->position.z-.15)<1e-8);
    CHECK(targets[0].normalDeltas && std::abs(targets[0].normalDeltas->at(0).normal.x-.3)<1e-8);
    CHECK(std::any_of(baseline.warnings.begin(),baseline.warnings.end(),[](const auto &w){return w.find("source morph")!=w.npos;}));
    auto editData=baseline.object.get().data();auto &smile=editData.meshes[0].topology->morphTargets[0];
    smile.name="Grin";const auto changedPoint=std::find_if(smile.deltas.begin(),smile.deltas.end(),[](const auto &v){return v.point==2;});
    CHECK(changedPoint!=smile.deltas.end());changedPoint->position.z+=.07;smile.normalDeltas->at(0).normal.x+=.05;
    auto edited=engine.createObject(editData);Object3DExternalOptions merge;merge.document="group-test";
    auto editor=engine.editObject(source);const auto report=editor.get().applyExternalEdits(source.get(),
        {baseline.object.get(),baseline.externalIdentity},{edited.get(),baseline.externalIdentity},merge);
    CHECK(report.morphs==1 && report.names==1);
    const auto &after=editor.get().object().get().data().meshes[0].topology->morphTargets;
    CHECK(after.size()==3 && after[0].name=="Grin" && after[2].name=="Grin");
    CHECK(after[0].sourceTarget==42 && after[2].sourceTarget==9 && after[1].name=="Blink");
    CHECK(std::abs(after[0].deltas[0].position.z-.17)<1e-8 && std::abs(after[2].deltas[0].position.z-.05)<1e-8);
    CHECK(std::abs(after[0].normalDeltas->at(0).normal.x-.15)<1e-8 && std::abs(after[2].normalDeltas->at(0).normal.x-.2)<1e-8);
    CHECK(editor.get().undo() && editor.get().redo());
    Object3DEngine destination;auto restored=importObject3DProject(destination.reference(),exportObject3DProject(editor));
    CHECK(restored.editor.get().object().get().data().meshes[0].topology->morphTargets[2].name=="Grin");
    Object3DTemplateDefinition definition;definition.base=source;definition.targets={{"height",0,{{1,{0,.2,0}}}}};
    auto layered=engine.editTemplate(engine.createTemplate(definition));layered.get().setTemplateValues(std::array{TemplateValue{"height",.5}});
    const auto layerReport=layered.get().applyExternalEdits(source.get(),{baseline.object.get(),baseline.externalIdentity},
        {edited.get(),baseline.externalIdentity},merge);
    CHECK(layerReport.layer && layered.get().layers().size()==1);
    CHECK(layered.get().object().get().data().meshes[0].topology->morphTargets[2].name=="Grin");
    layered.get().setTemplateValues(std::array{TemplateValue{"height",1}});
    CHECK(std::abs(layered.get().object().get().data().meshes[0].topology->points[1].position.y-.2)<1e-8);
    auto savedLayer=importObject3DProject(destination.reference(),exportObject3DProject(layered));
    CHECK(savedLayer.editor.get().object().get().data().meshes[0].topology->morphTargets[2].sourceTarget==9);
#if IIACTOROBJECT_HAS_NSCHARACTER
    auto character=std::make_shared<iiActorObject::NSCharacter::CharacterAsset>(source);
    CHECK(&character->data()==&source.get().data());
    iiActorObject::NSCharacter::CharacterEditor characterEditor(character);
    CHECK(characterEditor.applyExternalEdits(*character,{baseline.object.get(),baseline.externalIdentity},
        {edited.get(),baseline.externalIdentity},merge).morphs==1);
    CHECK(characterEditor.asset()->data().meshes[0].topology->morphTargets[2].name=="Grin");
    CHECK(characterEditor.undo() && characterEditor.redo());
#endif
    auto conflict=source.get().data();conflict.meshes[0].topology->morphTargets[2].name="Concurrent";
    rejects([&]{(void)applyObject3DExternalEdits(conflict,source.get(),{baseline.object.get(),baseline.externalIdentity},
        {edited.get(),baseline.externalIdentity},merge);});
    CHECK(conflict.meshes[0].topology->morphTargets[0].name=="Smile");
    auto collisionData=baseline.object.get().data();collisionData.meshes[0].topology->morphTargets[0].name="Blink";
    auto collision=engine.createObject(std::move(collisionData));auto beforeCollision=source.get().data();
    rejects([&]{(void)applyObject3DExternalEdits(beforeCollision,source.get(),{baseline.object.get(),baseline.externalIdentity},
        {collision.get(),baseline.externalIdentity},merge);});
    CHECK(beforeCollision.meshes[0].topology->morphTargets[0].name=="Smile");
    auto duplicateIdentity=baseline.externalIdentity;duplicateIdentity.meshes[0].morphs.push_back({0,"Other"});
    rejects([&]{(void)applyObject3DExternalEdits(beforeCollision,source.get(),{baseline.object.get(),baseline.externalIdentity},
        {edited.get(),duplicateIdentity},merge);});
    CHECK(beforeCollision.meshes[0].topology->morphTargets.size()==3);
    auto withLocal=source.get().data();TopologyMorph local;local.name="Smile";local.deltas={{0,{.03,0,0}}};
    local.normalDeltas=std::vector<TopologyNormalDelta>{{0,{.04,0,0}}};withLocal.meshes[0].topology->morphTargets.push_back(local);
    auto renamedLocal=withLocal;CHECK(applyObject3DExternalEdits(renamedLocal,source.get(),{baseline.object.get(),baseline.externalIdentity},
        {edited.get(),baseline.externalIdentity},merge).morphs==1);
    CHECK(renamedLocal.meshes[0].topology->morphTargets.back().name=="Grin");
    near(renamedLocal.meshes[0].topology->morphTargets.back().deltas[0].position,{.03,0,0});
    near(renamedLocal.meshes[0].topology->morphTargets[0].normalDeltas->at(0).normal,{.15,0,0});
    near(renamedLocal.meshes[0].topology->morphTargets.back().normalDeltas->at(0).normal,{.04,0,0});
    auto wrongBaselineData=baseline.object.get().data();
    wrongBaselineData.meshes[0].topology->morphTargets[0].normalDeltas->at(0).normal.x+=.2;
    auto wrongBaseline=engine.createObject(std::move(wrongBaselineData));auto unchanged=source.get().data();
    rejects([&]{(void)applyObject3DExternalEdits(unchanged,source.get(),{wrongBaseline.get(),baseline.externalIdentity},
        {edited.get(),baseline.externalIdentity},merge);});
    CHECK(unchanged.meshes[0].topology->morphTargets[0].name=="Smile");
    auto deletedData=baseline.object.get().data();deletedData.meshes[0].topology->morphTargets.erase(deletedData.meshes[0].topology->morphTargets.begin());
    auto deleted=engine.createObject(deletedData);auto deletedIdentity=baseline.externalIdentity;
    deletedIdentity.meshes[0].morphs.erase(deletedIdentity.meshes[0].morphs.begin());deletedIdentity.meshes[0].morphs[0].morph=0;
    rejects([&]{(void)applyObject3DExternalEdits(withLocal,source.get(),{baseline.object.get(),baseline.externalIdentity},
        {deleted.get(),deletedIdentity},merge);});
    CHECK(withLocal.meshes[0].topology->morphTargets.back().name=="Smile");
    auto forcedDeletion=withLocal;auto force=merge;force.overwriteConflicts=true;
    CHECK(applyObject3DExternalEdits(forcedDeletion,source.get(),{baseline.object.get(),baseline.externalIdentity},
        {deleted.get(),deletedIdentity},force).morphs==1);
    CHECK(forcedDeletion.meshes[0].topology->morphTargets.back().deltas.empty());
    auto locallyChanged=source.get().data();locallyChanged.meshes[0].topology->morphTargets[2].deltas[0].position.z+=.03;
    rejects([&]{(void)applyObject3DExternalEdits(locallyChanged,source.get(),{baseline.object.get(),baseline.externalIdentity},
        {deleted.get(),deletedIdentity},merge);});
    CHECK(std::abs(locallyChanged.meshes[0].topology->morphTargets[2].deltas[0].position.z-.08)<1e-8);
    auto current=source.get().data();CHECK(applyObject3DExternalEdits(current,source.get(),{baseline.object.get(),baseline.externalIdentity},
        {deleted.get(),deletedIdentity},merge).morphs==1);
    const auto &empty=current.meshes[0].topology->morphTargets;
    CHECK(empty[0].sourceTarget==42 && empty[2].sourceTarget==9 && empty[0].deltas.empty() && empty[2].deltas.empty());
    CHECK(empty[0].normalDeltas && empty[0].normalDeltas->empty() && empty[2].normalDeltas && empty[2].normalDeltas->empty());
    const auto authoring=importObject3DFbx(engine.reference(),exportObject3DFbx(source.get()));
    CHECK(authoring.object.get().data().meshes[0].topology->morphTargets.size()==3);
    auto idlessData=source.get().data();idlessData.meshes[0].topology->morphTargets[0].sourceTarget=NoTopologyIndex;
    idlessData.meshes[0].topology->morphTargets[2].sourceTarget=NoTopologyIndex;
    auto idless=engine.createObject(std::move(idlessData));const auto idlessBaseline=importObject3DFbx(engine.reference(),exportObject3DFbx(idless.get(),exportOptions),options);
    auto idlessEdit=idlessBaseline.object.get().data();auto &channel=idlessEdit.meshes[0].topology->morphTargets[0];channel.name="Grin";
    auto idlessPoint=std::find_if(channel.deltas.begin(),channel.deltas.end(),[](const auto &v){return v.point==2;});
    CHECK(idlessPoint!=channel.deltas.end());idlessPoint->position.z+=.02;auto idlessEdited=engine.createObject(std::move(idlessEdit));
    auto idlessCurrent=idless.get().data();CHECK(applyObject3DExternalEdits(idlessCurrent,idless.get(),
        {idlessBaseline.object.get(),idlessBaseline.externalIdentity},{idlessEdited.get(),idlessBaseline.externalIdentity},merge).morphs==1);
    const auto &idlessRecords=idlessCurrent.meshes[0].topology->morphTargets;
    CHECK(idlessRecords[0].name=="Grin"&&idlessRecords[2].name=="Grin"&&idlessRecords[0].sourceTarget==NoTopologyIndex&&idlessRecords[2].sourceTarget==NoTopologyIndex);
    CHECK(std::abs(idlessRecords[0].deltas[0].position.z-.12)<1e-8&&std::abs(idlessRecords[2].deltas[0].position.z-.05)<1e-8);
}
void groupedMorphsMixed()
{
    Object3DEngine engine;auto d=data();TopologyMorph second;second.name="Smile";second.sourceTarget=9;
    second.deltas={{2,{0,0,.05}}};second.normalDeltas=std::vector<TopologyNormalDelta>{{0,{.12,0,0}}};
    d.meshes[0].topology->morphTargets.push_back(second);auto source=engine.createObject(d);
    FbxExportOptions output;output.externalDocument="mixed-morph";
    FbxImportOptions input;input.restoreAuthoringMetadata=false;
    const auto baseline=importObject3DFbx(engine.reference(),exportObject3DFbx(source.get(),output),input);
    const auto vectorAt=[](const Object3DAsset &asset,std::size_t vertex)
    {
        const auto &morph=asset.data().meshes[0].geometry.morphTargets[0];
        const auto found=std::find_if(morph.deltas.begin(),morph.deltas.end(),[&](const auto &v){return v.vertex==vertex;});
        return found==morph.deltas.end()?Vector3{}:found->normal;
    };
    for(std::size_t i=0;i<4;++i)near(vectorAt(source.get(),i),vectorAt(baseline.object.get(),i));
    auto editedData=baseline.object.get().data();auto &morph=editedData.meshes[0].topology->morphTargets[0];
    CHECK(morph.normalDeltas);auto normal=std::find_if(morph.normalDeltas->begin(),morph.normalDeltas->end(),[](const auto &v){return v.layout==0;});
    if(normal==morph.normalDeltas->end())morph.normalDeltas->push_back({0,{.025,0,0}});else normal->normal.x+=.025;
    auto point=std::find_if(morph.deltas.begin(),morph.deltas.end(),[](const auto &v){return v.point==2;});
    CHECK(point!=morph.deltas.end());point->position.z+=.02;
    auto edited=engine.createObject(std::move(editedData));Object3DExternalOptions options;options.document="mixed-morph";
    auto current=source.get().data();const auto report=applyObject3DExternalEdits(current,source.get(),
        {baseline.object.get(),baseline.externalIdentity},{edited.get(),baseline.externalIdentity},options);
    CHECK(report.morphs==1);auto merged=engine.createObject(std::move(current));
    const auto &records=merged.get().data().meshes[0].topology->morphTargets;
    CHECK(records.size()==2 && records[0].sourceTarget==42 && records[1].sourceTarget==9 && records[0].normalDeltas);
    for(std::size_t i=0;i<4;++i)
    {
        auto expected=vectorAt(source.get(),i);if(i==0)expected.x+=.025;
        near(vectorAt(merged.get(),i),expected);
    }
    auto cancelling=data();TopologyMorph opposite;opposite.name="Smile";opposite.sourceTarget=9;
    opposite.deltas={{2,{0,0,-.1}}};cancelling.meshes[0].topology->morphTargets.push_back(opposite);
    auto zeroSource=engine.createObject(std::move(cancelling));
    const auto zero=importObject3DFbx(engine.reference(),exportObject3DFbx(zeroSource.get(),output),input);
    CHECK(zero.object.get().data().meshes[0].topology->morphTargets.size()==1);
    auto zeroEdit=zero.object.get().data();zeroEdit.meshes[0].topology->morphTargets[0].deltas={{2,{0,0,.04}}};
    auto zeroEdited=engine.createObject(std::move(zeroEdit));auto zeroCurrent=zeroSource.get().data();
    CHECK(applyObject3DExternalEdits(zeroCurrent,zeroSource.get(),{zero.object.get(),zero.externalIdentity},
        {zeroEdited.get(),zero.externalIdentity},options).morphs==1);
    CHECK(std::abs(zeroCurrent.meshes[0].topology->morphTargets[0].deltas[0].position.z-.14)<1e-8);
    CHECK(std::abs(zeroCurrent.meshes[0].topology->morphTargets[1].deltas[0].position.z+.1)<1e-8);
}
void emitGroupedMorphFbx()
{
    Object3DEngine engine;auto d=data();TopologyMorph blink;blink.name="Blink";blink.sourceTarget=7;
    blink.deltas={{0,{.01,0,0}}};TopologyMorph second;second.name="Smile";second.sourceTarget=9;
    second.deltas={{2,{0,0,.05}}};d.meshes[0].topology->morphTargets.push_back(blink);
    d.meshes[0].topology->morphTargets.push_back(second);auto source=engine.createObject(std::move(d));
    FbxExportOptions options;options.externalDocument="independent-group-test";
    for(const auto byte:exportObject3DFbx(source.get(),options))
        std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(byte);
    std::cout<<'\n';
}
void upsert()
{
    Scene s;Object3DTemplateDefinition definition;definition.base=s.source;definition.targets={{"height",0,{{2,{0,.2,0}}}}};
    auto editor=s.engine.editTemplate(s.engine.createTemplate(definition));auto &e=editor.get();auto d=s.baseline.get().data();d.meshes[0].topology->points[2].position.x+=.03;auto edited=s.engine.createObject(d);
    const auto report=e.applyExternalEdits(s.source.get(),s.base(),{edited.get(),s.ids},s.options);CHECK(report.layer);const auto id=*report.layer;
    e.setLayerWeight(id,.5);e.setLayerPointMasks(id,std::array{MeshPointMask{0,{0,0,1,0,0}}});e.renameLayer(id,"External work");
    e.moveControlPoints(0,std::array{TopologyPointMove{2,{0,0,.02}}});e.setTemplateValues(std::array{TemplateValue{"height",.5}});
    auto options=s.options;options.replaceLayer=id;
    CHECK(e.applyExternalEdits(s.source.get(),s.base(),{edited.get(),s.ids},options).layer==id);
    near(e.object().get().data().meshes[0].topology->points[2].position,{1.015,1.1,.02});
    CHECK(e.layers().size()==2&&e.layers()[0].id==id&&e.layers()[0].weight==.5&&e.layers()[0].label=="External work"&&e.layerPointMasks(id).size()==1);
    CHECK(e.applyExternalEdits(s.source.get(),s.base(),s.base(),options).changed());near(e.object().get().data().meshes[0].topology->points[2].position,{1,1.1,.02});
    CHECK(e.layers().size()==2&&e.layers()[0].id==id);CHECK(e.undo());near(e.object().get().data().meshes[0].topology->points[2].position,{1.015,1.1,.02});CHECK(e.redo());
    Object3DEngine destination;auto restored=importObject3DProject(destination.reference(),exportObject3DProject(editor));near(restored.editor.get().object().get().data().meshes[0].topology->points[2].position,{1,1.1,.02});
    const auto revision=e.revision();options.replaceLayer=999999;rejects([&]{(void)e.applyExternalEdits(s.source.get(),s.base(),s.base(),options);});CHECK(e.revision()==revision);
}
void morphAttributes()
{
    Object3DEngine engine;auto d=data();d.meshes[0].topology->morphTargets[0].normalDeltas=std::vector<TopologyNormalDelta>{{2,{.1,0,0}}};auto source=engine.createObject(d);const auto ids=makeObject3DExternalIdentity(d,"morph-test");Object3DExternalOptions options;options.document=ids.document;
    d.meshes[0].topology->morphTargets[0].name="Grin";d.meshes[0].topology->morphTargets[0].normalDeltas->at(0).normal.x+=.05;
    TopologyMorph blink;blink.name="Blink";d.meshes[0].topology->morphTargets.push_back(blink);auto edited=engine.createObject(d);auto editIds=ids;editIds.meshes[0].morphs.push_back({1,"Blink"});auto current=source.get().data();
    const auto report=applyObject3DExternalEdits(current,source.get(),{source.get(),ids},{edited.get(),editIds},options);CHECK(report.morphs==2&&report.names==1);
    const auto &m=current.meshes[0].topology->morphTargets;CHECK(m.size()==2&&m[0].name=="Grin"&&m[0].sourceTarget==42&&m[1].name=="Blink");near(m[0].normalDeltas->at(0).normal,{.15,0,0});
    d=source.get().data();d.meshes[0].topology->morphTargets.clear();auto deleted=engine.createObject(d);auto deletedIds=ids;deletedIds.meshes[0].morphs.clear();current=source.get().data();CHECK(applyObject3DExternalEdits(current,source.get(),{source.get(),ids},{deleted.get(),deletedIds},options).morphs==1);CHECK(current.meshes[0].topology->morphTargets[0].sourceTarget==42&&current.meshes[0].topology->morphTargets[0].deltas.empty()&&current.meshes[0].topology->morphTargets[0].normalDeltas->empty());
}
void baseNormals()
{
    Scene s;auto d=s.baseline.get().data();d.meshes[0].topology->normals[0]={.6,0,.8};
    auto edited=s.engine.createObject(d);auto current=s.source.get().data();
    auto report=applyObject3DExternalEdits(current,s.source.get(),s.base(),{edited.get(),s.ids},s.options);
    CHECK(report.changed()&&report.normals==1);near(current.meshes[0].topology->normals[0],{.6,0,.8});
    auto editor=s.engine.editObject(s.source);
    CHECK(editor.get().applyExternalEdits(s.source.get(),s.base(),{edited.get(),s.ids},s.options).changed());
    near(editor.get().object().get().data().meshes[0].topology->normals[0],{.6,0,.8});
    CHECK(editor.get().undo());near(editor.get().object().get().data().meshes[0].topology->normals[0],{0,0,1});
    CHECK(editor.get().redo());near(editor.get().object().get().data().meshes[0].topology->normals[0],{.6,0,.8});
    Object3DTemplateDefinition program;program.base=s.source;program.targets={{"height",0,{{2,{0,.2,0}}}}};
    auto layered=s.engine.editTemplate(s.engine.createTemplate(program));
    auto applied=layered.get().applyExternalEdits(s.source.get(),s.base(),{edited.get(),s.ids},s.options);
    CHECK(applied.layer);near(layered.get().object().get().data().meshes[0].topology->normals[0],{.6,0,.8});
    layered.get().setTemplateValues(std::array{TemplateValue{"height",.5}});
    near(layered.get().object().get().data().meshes[0].topology->normals[0],{.6,0,.8});
    Object3DEngine destination;const auto archive=exportObject3DProject(layered);CHECK(archive[8]==19);auto restored=importObject3DProject(destination.reference(),archive);
    near(restored.editor.get().object().get().data().meshes[0].topology->normals[0],{.6,0,.8});
    restored.editor.get().setTemplateValues({});
    near(restored.editor.get().object().get().data().meshes[0].topology->normals[0],{.6,0,.8});
}
void baseNormalsAdvanced()
{
    Scene s;auto edit=s.baseline.get().data();edit.meshes[0].topology->normals[0]={.6,0,.8};auto edited=s.engine.createObject(edit);
    Object3DTemplateDefinition program;program.base=s.source;program.targets={{"height",0,{{2,{0,.2,0}}}}};
    auto layered=s.engine.editTemplate(s.engine.createTemplate(program));
    auto first=layered.get().applyExternalEdits(s.source.get(),s.base(),{edited.get(),s.ids},s.options);CHECK(first.layer);
    const auto angle=std::atan2(.6,.8);
    layered.get().setLayerWeight(*first.layer,.5);
    near(layered.get().object().get().data().meshes[0].topology->normals[0],{std::sin(angle*.5),0,std::cos(angle*.5)});
    layered.get().setLayerWeight(*first.layer,1);
    const std::array mask{MeshPointMask{0,{1,0,0,0,0}}};layered.get().setLayerPointMasks(*first.layer,mask);
    near(layered.get().object().get().data().meshes[0].topology->normals[0],{std::sin(angle*.25),0,std::cos(angle*.25)});
    layered.get().setLayerPointMasks(*first.layer,{});
    auto moved=s.baseline.get().data();moved.meshes[0].topology->points[2].position.z+=.3;
    recomputeTopologyNormals(*moved.meshes[0].topology);
    Transform rotation;rotation.rotation=Quaternion::fromAxisAngle({0,1,0},.35);
    moved.meshes[0].topology->normals[0]=rotation.matrix().transformVector(moved.meshes[0].topology->normals[0]);
    auto movedEdited=s.engine.createObject(moved);auto canonical=s.source.get().data();
    const auto movedReport=applyObject3DExternalEdits(canonical,s.source.get(),s.base(),{movedEdited.get(),s.ids},s.options);
    CHECK(movedReport.positions==1&&movedReport.normals==1);near(canonical.meshes[0].topology->normals[0],moved.meshes[0].topology->normals[0]);
    auto expected=layered.get().object().get().data();
    (void)applyObject3DExternalEdits(expected,s.source.get(),s.base(),{movedEdited.get(),s.ids},s.options);
    CHECK(layered.get().applyExternalEdits(s.source.get(),s.base(),{movedEdited.get(),s.ids},s.options).changed());
    near(layered.get().object().get().data().meshes[0].topology->normals[0],expected.meshes[0].topology->normals[0]);
    Object3DEngine restoredEngine;auto restored=importObject3DProject(restoredEngine.reference(),exportObject3DProject(layered));
    near(restored.editor.get().object().get().data().meshes[0].topology->normals[0],expected.meshes[0].topology->normals[0]);
    CHECK(restored.editor.get().undo()&&restored.editor.get().redo());
    auto replacement=s.engine.editTemplate(s.engine.createTemplate(program));
    auto initial=replacement.get().applyExternalEdits(s.source.get(),s.base(),{edited.get(),s.ids},s.options);CHECK(initial.layer);
    replacement.get().setLayerWeight(*initial.layer,.5);
    auto alternate=s.baseline.get().data();alternate.meshes[0].topology->normals[0]={.8,0,.6};auto alternateEdit=s.engine.createObject(alternate);
    auto replaceOptions=s.options;replaceOptions.replaceLayer=*initial.layer;
    CHECK(replacement.get().applyExternalEdits(s.source.get(),s.base(),{alternateEdit.get(),s.ids},replaceOptions).layer==initial.layer);
    const auto replacementAngle=std::atan2(.8,.6)*.5;
    near(replacement.get().object().get().data().meshes[0].topology->normals[0],{std::sin(replacementAngle),0,std::cos(replacementAngle)});
    CHECK(replacement.get().undo()&&replacement.get().redo());
    auto ordinary=s.engine.editTemplate(s.engine.createTemplate(program));auto ordinaryEdit=ordinary.get().object().get().data();
    ordinaryEdit.meshes[0].topology->normals[0]={.6,0,.8};ordinary.get().replace(std::move(ordinaryEdit),"Authored normal");
    ordinary.get().setTemplateValues(std::array{TemplateValue{"height",.5}});
    near(ordinary.get().object().get().data().meshes[0].topology->normals[0],{.6,0,.8});
    auto opposite=s.baseline.get().data();opposite.meshes[0].topology->normals[0]={0,0,-1};auto oppositeEdit=s.engine.createObject(opposite);
    auto oppositeResult=s.source.get().data();CHECK(applyObject3DExternalEdits(oppositeResult,s.source.get(),s.base(),{oppositeEdit.get(),s.ids},s.options).normals==1);
    near(oppositeResult.meshes[0].topology->normals[0],{0,0,-1});
    auto bad=s.baseline.get().data();bad.meshes[0].topology->normals[0]={-.6,0,.8};auto badBaseline=s.engine.createObject(bad);
    auto unchanged=s.source.get().data();rejects([&]{(void)applyObject3DExternalEdits(unchanged,s.source.get(),{badBaseline.get(),s.ids},{edited.get(),s.ids},s.options);});
    near(unchanged.meshes[0].topology->normals[0],{0,0,1});
    auto split=s.baseline.get().data();auto &topology=*split.meshes[0].topology;auto definition=topology.layout->definition();
    definition.normalCount=2;definition.layouts[definition.polygons[0][1]].normal=1;
    topology.normals.push_back({.6,0,.8});topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);
    auto splitEdited=s.engine.createObject(split);
    CHECK(!applyObject3DExternalEdits(unchanged,s.source.get(),{splitEdited.get(),s.ids},{splitEdited.get(),s.ids},s.options).changed());
    rejects([&]{(void)applyObject3DExternalEdits(unchanged,s.source.get(),s.base(),{splitEdited.get(),s.ids},s.options);});
    near(unchanged.meshes[0].topology->normals[0],{0,0,1});
    Object3DEngine renderEngine;auto render=s.source.get().data();render.meshes[0].topology.reset();auto renderSource=renderEngine.createObject(render);
    const auto ids=makeObject3DExternalIdentity(renderSource.get().data(),"render-normal");Object3DExternalOptions options;options.document=ids.document;
    render=renderSource.get().data();render.meshes[0].geometry.vertices[0].normal={.6,0,.8};auto renderEdited=renderEngine.createObject(render);
    auto renderCurrent=renderSource.get().data();const auto report=applyObject3DExternalEdits(renderCurrent,renderSource.get(),{renderSource.get(),ids},{renderEdited.get(),ids},options);
    CHECK(report.normals==1);near(renderCurrent.meshes[0].geometry.vertices[0].normal,{.6,0,.8});
}
void emitBaseNormalFbx()
{
    Scene s;FbxExportOptions out;out.externalDocument=s.ids.document;FbxImportOptions in;in.restoreAuthoringMetadata=false;
    auto baseline=importObject3DFbx(s.engine.reference(),exportObject3DFbx(s.source.get(),out),in);
    auto data=baseline.object.get().data();for(auto &normal:data.meshes[0].topology->normals)normal={.6,0,.8};
    auto edit=s.engine.createObject(std::move(data));auto edited=importObject3DFbx(s.engine.reference(),exportObject3DFbx(edit.get(),out),in);
    auto current=s.source.get().data();CHECK(applyObject3DExternalEdits(current,s.source.get(),
        {baseline.object.get(),baseline.externalIdentity},{edited.object.get(),edited.externalIdentity},s.options).normals==1);
    auto merged=s.engine.createObject(std::move(current));
    for(const auto byte:exportObject3DFbx(merged.get(),out))std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<unsigned>(byte);
    std::cout<<'\n';
}
void baseNormalsProjectFixture(const std::filesystem::path &path)
{
    Scene s;Object3DTemplateDefinition program;program.base=s.source;auto editor=s.engine.editTemplate(s.engine.createTemplate(program));
    auto data=s.baseline.get().data();data.meshes[0].topology->normals[0]={.6,0,.8};auto edited=s.engine.createObject(data);
    CHECK(editor.get().applyExternalEdits(s.source.get(),s.base(),{edited.get(),s.ids},s.options).normals==1);
    const auto bytes=exportObject3DProject(editor);CHECK(bytes[8]==19);std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path,std::ios::binary);CHECK(file.good());file.write(reinterpret_cast<const char *>(bytes.data()),bytes.size());CHECK(file.good());
}
void reordered()
{
    Scene s;auto d=s.baseline.get().data();auto ids=s.ids;auto &mesh=d.meshes[0];auto &topology=*mesh.topology;std::reverse(topology.points.begin(),topology.points.end());std::reverse(ids.meshes[0].points.begin(),ids.meshes[0].points.end());
    auto definition=topology.layout->definition();for(auto &layout:definition.layouts)layout.position=4-layout.position;topology.layout=MeshTopologyLayout::create(std::move(definition),topology.points);for(auto &m:topology.morphTargets)for(auto &delta:m.deltas)delta.point=4-delta.point;
    std::swap(d.bones[1],d.bones[2]);auto remap=[](BoneIndex b){return b==1?2:b==2?1:b;};for(auto &bone:d.bones)if(bone.parent!=NoBone)bone.parent=remap(bone.parent);for(auto &p:topology.points)for(auto &w:p.weights)w.bone=remap(w.bone);for(auto &id:ids.bones)id.bone=remap(id.bone);for(auto &semantic:d.semantics)semantic.bone=remap(semantic.bone);
    topology.points[1].position.x+=.02;auto edited=s.engine.createObject(d);auto current=s.source.get().data();const auto report=applyObject3DExternalEdits(current,s.source.get(),s.base(),{edited.get(),ids},s.options);CHECK(report.positions==1&&!report.parents&&!report.names&&!report.weightRows&&!report.morphs);near(current.meshes[0].topology->points[3].position,{.02,1,0});CHECK(current.bones[1].name=="hand"&&current.bones[1].parent==2);
}
void malformedProtocol()
{
    Scene s;FbxExportOptions options;options.externalDocument=s.ids.document;auto bytes=exportObject3DFbx(s.source.get(),options);const std::string_view ids="0,1,2,3,4";auto found=std::search(bytes.begin(),bytes.end(),ids.begin(),ids.end());CHECK(found!=bytes.end());found[8]='x';const auto count=s.engine.objectCount();rejects([&]{(void)importObject3DFbx(s.engine.reference(),bytes);});CHECK(s.engine.objectCount()==count);
}
void surfaceChain()
{
    Scene s;auto reference=s.source.get().data();for(unsigned m=1;m<3;++m){auto mesh=reference.meshes[0];mesh.name="Follower "+std::to_string(m);for(auto &p:mesh.topology->points)p.position.z+=m*.02;reference.meshes.push_back(std::move(mesh));}
    const std::array bindings{bindMeshSurface(reference,0,1),bindMeshSurface(reference,1,2)};auto final=reference;final.meshes[0].topology->points[2].position.z+=.03;final.meshes[1].topology->points[2].position.x+=.01;final.meshes[2].topology->points[2].position.y-=.02;
    auto actual=final;unapplyMeshSurfaceBindings(actual,reference,bindings);applyMeshSurfaceBindings(actual,reference,bindings);for(std::size_t m=0;m<3;++m)for(std::size_t p=0;p<5;++p)near(actual.meshes[m].topology->points[p].position,final.meshes[m].topology->points[p].position);
}
void images(const std::filesystem::path &checker,const std::filesystem::path &blue)
{
    auto read=[](const auto &path){std::ifstream file(path,std::ios::binary);CHECK(file.good());return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(file)),{});};
    Object3DEngine engine;auto d=data();const auto png=read(checker);d.images.emplace_back("Source",png);d.materials[0].baseColorImage=0;
    auto source=engine.createObject(d);const auto ids=makeObject3DExternalIdentity(d,"image-edit");Object3DExternalOptions options;options.document=ids.document;
    d.images[0]=Object3DImage("Imported name",png);d.meshes[0].topology->points[2].position.x+=.03;auto edited=engine.createObject(d);auto current=source.get().data();
    CHECK(applyObject3DExternalEdits(current,source.get(),{source.get(),ids},{edited.get(),ids},options).positions==1);near(current.meshes[0].topology->points[2].position,{1.03,1,0});
    CHECK(current.images[0].name()=="Source");
    d.images[0]=Object3DImage("Replacement",read(blue));auto replaced=engine.createObject(d);
    rejects([&]{(void)applyObject3DExternalEdits(current,source.get(),{source.get(),ids},{replaced.get(),ids},options);});
    d.materials[0].baseColorImage.reset();auto removed=engine.createObject(d);
    rejects([&]{(void)applyObject3DExternalEdits(current,source.get(),{source.get(),ids},{removed.get(),ids},options);});
    near(current.meshes[0].topology->points[2].position,{1.03,1,0});CHECK(current.images[0]==source.get().data().images[0]);
}
int main(int argc,char **argv)
{
    try{if(argc!=2&&argc!=3&&argc!=4)return 2;const std::string_view c=argv[1];if(c=="images"&&argc==4)images(argv[2],argv[3]);else if(c=="base_normals_project_fixture"&&argc==3)baseNormalsProjectFixture(argv[2]);else if((c=="inspect_glb_accept"||c=="inspect_glb_reject"||c=="inspect_glb_reject_file"||c=="inspect_glb_edit"||c=="inspect_glb_rename"||c=="inspect_glb_multi")&&argc==3)inspectAutomaticGlb(argv[2],c);else if(c=="changes")changes();else if(c=="noop")noop();else if(c=="identity")identity();else if(c=="conflicts")conflicts();else if(c=="attributes")attributes();else if(c=="layers")layers();else if(c=="surfaces")surfaces();else if(c=="material_surface")materialSurface();else if(c=="fbx_identity")fbxIdentity();else if(c=="glb_triangles"){glbTriangles();glbTriangleSeams();}else if(c=="glb_auto")glbAutomaticIdentity();else if(c=="emit_glb_auto"||c=="emit_glb_multi"){emitAutomaticGlb(c=="emit_glb_multi");return 0;}else if(c=="grouped_morphs")groupedMorphs();else if(c=="grouped_morphs_mixed")groupedMorphsMixed();else if(c=="emit_grouped_morph_fbx"){emitGroupedMorphFbx();return 0;}else if(c=="emit_base_normal_fbx"){emitBaseNormalFbx();return 0;}else if(c=="upsert")upsert();else if(c=="morph_attributes")morphAttributes();else if(c=="base_normals")baseNormals();else if(c=="base_normals_advanced")baseNormalsAdvanced();else if(c=="reordered")reordered();else if(c=="malformed_protocol")malformedProtocol();else if(c=="surface_chain")surfaceChain();else return 2;std::cout<<"Passed "<<c<<'\n';}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
