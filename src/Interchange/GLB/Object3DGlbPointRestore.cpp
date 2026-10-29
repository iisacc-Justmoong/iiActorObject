#include "Object3DGlbPointRestore.h"
#include "Object3DGlbGeometryBudget.h"
#include "Object3DGlbPointIdentity.h"
#include "Object3DGlbPointRestoreCore.h"
#include "Object3DGlbSkin.h"
#include "Object3DGlbImportOptions.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail::glb_import
{
bool restorePointIdentity(const Value& root,const Value& metadata,const GlbDocument& document,
                          const GlbImportOptions& options,GeometryBudgets& budget,const Skin& skin,
                          const ufbx_matrix& transform,MeshTopology& topology,MeshTopologyDefinition& definition,
                          const std::vector<std::uint32_t>& ids,bool invalid,bool externalProvenance,
                          std::vector<std::string>& warnings)
{
    try
    {
        if(invalid||ids.size()!=topology.points.size()||integer(required(metadata,"version"))!=1)
            throw std::invalid_argument("Invalid point identity attributes");
        const auto bytes=document.bufferView(integer(required(metadata,"bufferView")));
        auto saved=object3d_detail::decodeGlbPointIdentity(bytes,options.maximumVertices,
            options.maximumMorphsPerMesh,options.maximumMorphDeltas,options.maximumInfluencesPerVertex,options.maximumBones);
        if(saved.targetNames.size()!=topology.morphTargets.size())throw std::invalid_argument("Stale point identity morph count");
        for(std::size_t i=0;i<saved.targetNames.size();++i)
            if(saved.targetNames[i]!=topology.morphTargets[i].name)throw std::invalid_argument("Stale point identity morph names");
        std::vector<BoneIndex> sidecarToBone=skin.sourceToBone;
        if(skin.present)
        {
            const auto& skins=list(root,"skins");const auto& nodes=list(root,"nodes");
            if(skin.sourceIndex>=skins.size()||saved.boneNames.size()!=skin.sourceToBone.size())
                throw std::invalid_argument("Stale point identity skin");
            const auto& joints=array(required(skins[skin.sourceIndex],"joints"));
            if(joints.size()!=saved.boneNames.size())throw std::invalid_argument("Stale point identity joint count");
            // Token-bearing exports identify the sidecar's original joint order
            // independently of the current skin slot order and node names.
            bool mapped=false;
            if(externalProvenance)
            {
                try
                {
                    std::vector<BoneIndex> candidate(joints.size(),NoBone);
                    for(std::size_t i=0;i<joints.size();++i)
                    {
                        const auto node=integer(joints[i]);if(node>=nodes.size())throw std::invalid_argument("Invalid point identity joint");
                        const auto *value=optionalObjectField(field(nodes[node],"extras"),"iiActorObjectSourceBone");
                        if(!value)throw std::invalid_argument("Missing point identity joint ID");
                        const auto original=integer(*value);
                        if(original>=candidate.size()||candidate[original]!=NoBone)
                            throw std::invalid_argument("Invalid point identity joint ID");
                        candidate[original]=skin.sourceToBone[i];
                    }
                    sidecarToBone=std::move(candidate);mapped=true;
                }
                catch(const std::invalid_argument&){}
                catch(const std::out_of_range&){}
            }
            if(!mapped)for(std::size_t i=0;i<joints.size();++i)
            {
                const auto node=integer(joints[i]);if(node>=nodes.size())throw std::invalid_argument("Invalid point identity joint");
                const auto* n=field(nodes[node],"name");
                if(!n||text(*n)!=saved.boneNames[i])throw std::invalid_argument("Stale point identity joint names");
            }
        }
        else if(!saved.boneNames.empty()||!saved.emptyWeights.empty())
            throw std::invalid_argument("Stale point identity skin binding");
        if(budget.identityPoints>options.maximumVertices||saved.pointCount>options.maximumVertices-budget.identityPoints||
           budget.identityMorphs>options.maximumMorphDeltas||saved.morphs.size()>options.maximumMorphDeltas-budget.identityMorphs)
            throw std::length_error("GLB point identity aggregate budget");

        GlbPointRestoreOptions planOptions;
        std::copy_n(transform.v,12,planOptions.affine.begin());
        planOptions.sidecarToBone=sidecarToBone;
        planOptions.rigidBone=skin.rigidBone;planOptions.skinned=skin.present;
        planOptions.maximumPoints=options.maximumVertices;
        planOptions.maximumMorphDeltas=options.maximumMorphDeltas;
        auto plan=planGlbPointRestore(saved,topology,definition,ids,planOptions);
        for(std::size_t i=0;i<definition.layouts.size();++i)definition.layouts[i].position=plan.layoutPoints[i];
        topology.points=std::move(plan.points);
        for(std::size_t i=0;i<plan.morphs.size();++i)topology.morphTargets[i].deltas=std::move(plan.morphs[i]);
        budget.identityPoints+=saved.pointCount;budget.identityMorphs+=saved.morphs.size();
        return true;
    }
    catch(const std::invalid_argument&)
    {warnings.emplace_back("GLB source point identity was invalid or conflicted with standard mesh data; standard points retained");}
    catch(const std::out_of_range&)
    {warnings.emplace_back("GLB source point identity was invalid or conflicted with standard mesh data; standard points retained");}
    catch(const std::length_error&)
    {warnings.emplace_back("GLB source point identity exceeded its limits; standard points retained");}
    return false;
}
}
