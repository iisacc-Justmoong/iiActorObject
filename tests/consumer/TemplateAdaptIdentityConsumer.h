#pragma once
#include <iiActorObject/Features.h>
#include <iiActorObject/Object3DObj.h>
#include <iiActorObject/Object3DTemplateAdapt.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterTemplate.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>

// Uses installed public headers and the staged library in both configurations.
inline int templateAdaptIdentityConsumer()
{
    using namespace iiActorObject;
    Object3DEngine engine;
    auto source=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "v 0 0 .000001\nv 1 0 .000001\nv 0 1 .000001\nf 1 2 3\nf 4 5 6\n");
    source.kind=Object3DKind::SkeletalMesh;source.bones={{"first",NoBone,{}},{"second",0,{}}};
    for(std::size_t i=0;i<6;++i)source.meshes[0].topology->points[i].weights={{static_cast<BoneIndex>(i<3?0:1),1}};
    const std::array joints{TemplateJointBinding{0,{0,{0}},{0,{2}},{}}};
    bindObject3DTemplateJoints(source,joints);
    Object3DTemplateDefinition definition;definition.base=engine.createObject(source);
    definition.joints.assign(joints.begin(),joints.end());
    definition.targets={{"first",0,{{0,{.02,0,0}},{1,{.02,0,0}},{2,{.02,0,0}}}}};
    const auto program=engine.createTemplate(std::move(definition));
    auto target=source;target.kind=Object3DKind::StaticMesh;target.bones.clear();target.semantics.clear();
    for(std::size_t i=0;i<6;++i)
    {auto &point=target.meshes[0].topology->points[i];point.position.z+=i<3?.0000008:-.0000008;point.weights.clear();}
    for(auto &vertex:target.meshes[0].geometry.vertices)vertex.weights.clear();
    const auto input=engine.createObject(std::move(target));TemplateAdaptOptions options;
    options.maximumDistance=.001;options.rig.projection.maximumSurfaceDistance=.001;
    const auto result=adaptObject3DTemplate(program,input,options);
    const auto changed=result.program.get().snapshot(std::array{TemplateValue{"first",1}});
    for(std::size_t i=0;i<6;++i)
    {
        const auto &point=changed.meshes[0].topology->points[i];
        if(point.weights.size()!=1||point.weights[0].bone!=(i<3?0:1))return 1;
        if(std::abs(point.position.x-input.get().data().meshes[0].topology->points[i].position.x-(i<3?.02:0))>1e-9)return 2;
    }
    auto reordered=input.get().data();auto &topology=*reordered.meshes[0].topology;
    const std::array<std::uint32_t,6> map{3,4,5,0,1,2};const auto original=topology.points;
    for(std::size_t i=0;i<map.size();++i){topology.points[map[i]]=original[i];topology.points[map[i]].position.z+=.0001;}
    auto layout=topology.layout->definition();for(auto &entry:layout.layouts)entry.position=map[entry.position];
    const auto triangles=topology.layout->triangles(),faces=topology.layout->trianglePolygons();
    topology.layout=MeshTopologyLayout::restore(std::move(layout),topology.points,
        {triangles.begin(),triangles.end()},{faces.begin(),faces.end()});
    const auto reorderedInput=engine.createObject(std::move(reordered));TemplateAdaptMatchingOptions matching;
    matching.maximumPointDistance=.001;
    const auto reorderedResult=adaptObject3DTemplateWithMatching(program,reorderedInput,options,matching);
    if(reorderedResult.program.get().definition().base.get().data().meshes[0].topology->points[map[0]].weights[0].bone!=0)return 4;
    auto shuffled=source;shuffled.kind=Object3DKind::StaticMesh;shuffled.bones.clear();shuffled.semantics.clear();
    auto &shuffledTopology=*shuffled.meshes[0].topology;const auto originalPoints=shuffledTopology.points;
    for(std::size_t i=0;i<map.size();++i)
    {shuffledTopology.points[map[i]]=originalPoints[i];shuffledTopology.points[map[i]].weights.clear();}
    auto shuffledLayout=shuffledTopology.layout->definition();
    for(auto &entry:shuffledLayout.layouts)entry.position=map[entry.position];
    std::reverse(shuffledLayout.polygons.begin(),shuffledLayout.polygons.end());
    std::reverse(shuffledLayout.polygonGroups.begin(),shuffledLayout.polygonGroups.end());
    std::reverse(shuffledLayout.polygonMaterials.begin(),shuffledLayout.polygonMaterials.end());
    for(auto &face:shuffledLayout.polygons)std::rotate(face.begin(),face.begin()+1,face.end());
    shuffledTopology.layout=MeshTopologyLayout::create(std::move(shuffledLayout),shuffledTopology.points);
    for(auto &vertex:shuffled.meshes[0].geometry.vertices)vertex.weights.clear();
    const auto shuffledInput=engine.createObject(std::move(shuffled));matching.maximumPointDistance=0;
    const auto shuffledResult=adaptObject3DTemplateWithMatching(program,shuffledInput,options,matching);
    for(std::size_t i=0;i<map.size();++i)
    {
        const auto &point=shuffledResult.program.get().definition().base.get().data().meshes[0].topology->points[map[i]];
        if(point.weights.size()!=1||point.weights[0].bone!=(i<3?0:1))return 5;
    }
    auto collision=source;collision.kind=Object3DKind::StaticMesh;collision.bones.clear();collision.semantics.clear();
    auto &collisionTopology=*collision.meshes[0].topology;const auto collisionPoints=collisionTopology.points;
    for(std::size_t i=0;i<map.size();++i)
    {collisionTopology.points[map[i]]=collisionPoints[i];collisionTopology.points[map[i]].weights.clear();}
    auto collisionLayout=collisionTopology.layout->definition();
    for(auto &entry:collisionLayout.layouts)entry.position=map[entry.position];
    std::reverse(collisionLayout.polygons.begin(),collisionLayout.polygons.end());
    std::reverse(collisionLayout.polygonGroups.begin(),collisionLayout.polygonGroups.end());
    std::reverse(collisionLayout.polygonMaterials.begin(),collisionLayout.polygonMaterials.end());
    collisionTopology.layout=MeshTopologyLayout::create(std::move(collisionLayout),collisionTopology.points);
    for(auto &vertex:collision.meshes[0].geometry.vertices)vertex.weights.clear();
    const auto collisionInput=engine.createObject(std::move(collision));TemplateAdaptCorrespondenceOptions correspondence;
    correspondence.pointIdentity=TemplatePointIdentityPolicy::RequireUniqueGeometry;
    correspondence.matching.maximumPointDistance=0;
    const auto collisionResult=adaptObject3DTemplateWithCorrespondence(program,collisionInput,options,correspondence);
    for(std::size_t i=0;i<map.size();++i)
    {
        const auto &point=collisionResult.program.get().definition().base.get().data().meshes[0].topology->points[map[i]];
        if(point.weights.size()!=1||point.weights[0].bone!=(i<3?0:1))return 7;
    }
#if IIACTOROBJECT_HAS_NSCHARACTER
    const NSCharacter::CharacterTemplate character(program);
    matching.maximumPointDistance=.001;
    const auto wrapped=character.adaptMeshWithMatching(reorderedInput,options,matching);
    if(wrapped.program.get().definition().base.get().data().meshes[0].topology->points[map[0]].weights[0].bone!=0)return 3;
    matching.maximumPointDistance=0;
    const auto wrappedShuffled=character.adaptMeshWithMatching(shuffledInput,options,matching);
    if(wrappedShuffled.program.get().definition().base.get().data().meshes[0].topology->points[map[3]].weights[0].bone!=1)return 6;
    const auto wrappedCollision=character.adaptMeshWithCorrespondence(collisionInput,options,correspondence);
    if(wrappedCollision.program.get().definition().base.get().data().meshes[0].topology->points[map[3]].weights[0].bone!=1)return 8;
#endif
    return 0;
}
