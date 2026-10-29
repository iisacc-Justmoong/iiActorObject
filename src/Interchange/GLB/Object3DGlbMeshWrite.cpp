#include "Object3DGlbMeshWrite.h"
#include "Object3DGlbBuffer.h"
#include "Object3DGlbFbxTangentRoute.h"
#include "Object3DGlbMaterialPlan.h"
#include "Object3DGlbPrimitivePlan.h"
#include "Object3DGlbTangentPlan.h"
#include "Object3DGlbTangentWrite.h"
#include "Object3DGlbSharedMeshWrite.h"
#include "Object3DGlbMeshExtrasWrite.h"
#include "Object3DMaterial.h"
#include "Object3DMesh.h"
#include "Skeleton.h"
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace iiActorObject::object3d_detail::glb_export
{
std::string writeGlbMesh(Buffer &buffer,const Object3DMesh &mesh,GlbMaterialPlan &materialPlan,
    std::span<const Bone> bones,bool hasRig,bool external,std::size_t sourceMesh)
{
    if(external&&!mesh.topology)throw std::invalid_argument("GLB external edits require source topology");
    if(hasRig&&bones.empty())throw std::invalid_argument("GLB skinned mesh requires bones");
    if(hasRig&&bones.size()>65535)throw std::invalid_argument("glTF joint index exceeds 16 bits");
    const auto &g = mesh.geometry;
    const auto primitivePlan=planGlbPrimitives(mesh,materialPlan.materials());
    std::size_t influenceSets = 1;
    for (const auto &v : g.vertices)
        influenceSets = std::max(influenceSets, v.weights.size()/4 + (v.weights.size()%4 != 0));
    GlbFbxTangentSelector fbxSelector(mesh);
    std::optional<std::pair<std::string,std::string>> shared;
    const auto sharedData=[&]() -> const std::pair<std::string,std::string>&
    {
        if(!shared)shared=writeGlbSharedMeshData(buffer,mesh,hasRig,influenceSets);
        return *shared;
    };
    auto s=stream();s<<"{\"name\":"<<quote(mesh.name)<<",\"primitives\":[";bool firstPrimitive=true;
    std::size_t authoredWork=0;
    for(const auto &[key,part]:primitivePlan.parts)
    {
        const auto [material,useAuthored]=key;
        if(!firstPrimitive)s<<',';
        firstPrimitive=false;
        const auto exportedMaterial=materialPlan.materialFor(mesh,material);const auto &m=materialPlan.materials()[material];
        if(useAuthored)
        {
            const auto plan=object3d_detail::planAuthoredGlbTangents(mesh,part.corners,primitivePlan.tangentByLayout,authoredWork);
            s<<writeGlbTangentPrimitive(buffer,mesh,plan,hasRig,influenceSets,exportedMaterial);
            continue;
        }
        const auto writeFallback=[&](const GlbTrianglePart &fallbackPart,std::span<const std::string_view> notes,
                                     std::optional<std::size_t> unboundUv)
        {
            std::string_view tangentFallback;
            if(m.normalImage)
            {
                const auto index=glbMaterialUvIndex(mesh,m.normalUvSet);
                const std::span<const TextureCoordinate> uv=index==0?std::span<const TextureCoordinate>(mesh.textureCoordinates):
                    std::span<const TextureCoordinate>(mesh.additionalUvSets[index-1].coordinates);
                if(auto plan=object3d_detail::planGlbTangents(mesh,fallbackPart.corners,uv,tangentFallback))
                {
                    s<<writeGlbTangentPrimitive(buffer,mesh,*plan,hasRig,influenceSets,exportedMaterial,
                                        fallbackPart.authoredStatus,notes,unboundUv);
                    return;
                }
            }
            const auto &[attributes,targets]=sharedData();
            std::vector<std::uint8_t> triangles;for(const auto index:fallbackPart.vertices)u32(triangles,index);
            const auto indices=buffer.accessor(buffer.view(std::move(triangles),34963),5125,fallbackPart.vertices.size(),"SCALAR");
            s<<"{\"attributes\":"<<attributes<<",\"indices\":"<<indices<<",\"material\":"<<exportedMaterial<<targets;
            writeGlbTangentExtras(s,tangentFallback,fallbackPart.authoredStatus,notes,unboundUv);
            s<<'}';
        };
        const auto normalUv=m.normalImage?std::optional<std::size_t>{glbMaterialUvIndex(mesh,m.normalUvSet)}:std::nullopt;
        auto route=fbxSelector.select(normalUv);
        if(route.layer&&route.projection)
        {
            auto split=object3d_detail::planFbxGlbTangents(mesh,*route.layer,part.corners,
                                                           route.projection->triangleToPolygon,route.notes);
            if(!split.authored.sourceVertices.empty())
            {
                s<<writeGlbTangentPrimitive(buffer,mesh,split.authored,hasRig,influenceSets,exportedMaterial,
                                            part.authoredStatus,split.authoredNotes,route.unboundUv);
                if(!split.fallbackCorners.empty())
                {
                    s<<',';GlbTrianglePart remainder;remainder.corners=std::move(split.fallbackCorners);
                    remainder.authoredStatus=part.authoredStatus;remainder.vertices.reserve(remainder.corners.size());
                    for(const auto corner:remainder.corners)remainder.vertices.push_back(g.triangles.at(corner));
                    writeFallback(remainder,split.fallbackNotes,route.unboundUv);
                }
                continue;
            }
            route.notes=std::move(split.fallbackNotes);
        }
        writeFallback(part,route.notes,route.unboundUv);
    }
    s << "]";
    writeGlbMeshExtras(s,buffer,mesh,hasRig?bones:std::span<const Bone>{},external,sourceMesh);
    s << '}';
    return s.str();
}
} // namespace iiActorObject::object3d_detail::glb_export
