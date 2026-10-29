#pragma once
#include <cmath>
#include <iiActorObject/Features.h>
#include <iiActorObject/Object3DInterchange.h>
#include <iiActorObject/Object3DObj.h>
#if IIACTOROBJECT_HAS_NSCHARACTER
#include <iiActorObject/NSCharacter/CharacterInterchange.h>
#endif

inline int glbExternalIdentityConsumer()
{
    using namespace iiActorObject;
    Object3DEngine engine;
    auto data=parseObject3DObj("v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nf 1 2 3 4\n");
    data.kind=Object3DKind::SkeletalMesh;
    data.bones={{"root",NoBone,{}}};
    for(auto &point:data.meshes[0].topology->points)point.weights={{0,1}};
    const auto source=engine.createObject(std::move(data));
    const GlbExportOptions exportOptions{"installed-glb-external"};
    const auto baseline=importObject3DGlb(engine.reference(),exportObject3DGlb(source.get(),exportOptions));
    const auto baseIdentity=makeObject3DGlbExternalIdentity(source.get(),baseline,exportOptions.externalDocument);
    auto changed=source.get().data();changed.meshes[0].topology->points[2].position.z+=.02;
    const auto editedSource=engine.createObject(std::move(changed));
    const auto edited=importObject3DGlb(engine.reference(),exportObject3DGlb(editedSource.get(),exportOptions));
    const auto editedIdentity=makeObject3DGlbExternalIdentity(source.get(),edited,exportOptions.externalDocument);
    auto result=source.get().data();Object3DExternalOptions options;options.document=exportOptions.externalDocument;
    const auto report=applyObject3DExternalEdits(result,source.get(),
        {baseline.object.get(),baseIdentity},{edited.object.get(),editedIdentity},options);
#if IIACTOROBJECT_HAS_NSCHARACTER
    const NSCharacter::CharacterAsset character(source);
    const auto characterImport=NSCharacter::importGlb(engine.reference(),NSCharacter::exportGlb(character,exportOptions));
    const auto characterIdentity=NSCharacter::makeGlbExternalIdentity(character,characterImport,exportOptions.externalDocument);
    if(characterIdentity.meshes.size()!=1||characterIdentity.meshes[0].polygons.size()!=2)return 62;
#endif
    return baseIdentity.meshes.size()==1 &&
        baseIdentity.meshes[0].topology==ExternalMeshTopology::RenderTriangles &&
        baseIdentity.meshes[0].polygons.size()==2 && report.positions==1 &&
        std::abs(result.meshes[0].topology->points[2].position.z-.02)<1e-6 ? 0 : 61;
}
