#include "Object3DProjectVersion.h"
#include "Object3DProjectArchive.h"
#include <algorithm>

namespace iiActorObject::object3d_detail
{
std::uint32_t selectProjectVersion(const ProjectArchive &graph)
{
    const bool evaluatedLayers=std::any_of(graph.states.begin(),graph.states.end(),[](const auto &state){return std::any_of(state.layers.begin(),state.layers.end(),[](const auto &layer){return layer.info.evaluation!=Object3DLayerEvaluation::Deferred;});});
    const bool fairing=std::any_of(graph.programs.begin(),graph.programs.end(),[](const auto &p){return std::any_of(p.definition.fittings.begin(),p.definition.fittings.end(),[](const auto &rule){return rule.fitting.options().fairing!=SurfaceFairingOptions{};});});
    const bool guardedFairing=fairing&&std::any_of(graph.programs.begin(),graph.programs.end(),[](const auto &p){return std::any_of(p.definition.fittings.begin(),p.definition.fittings.end(),[](const auto &rule){return rule.fitting.options().fairing.shapePolicy==SurfaceFairingShapePolicy::PreserveMaximumEdgeStrain;});});
    const bool referencePose=std::any_of(graph.assets.begin(),graph.assets.end(),[](const auto &asset){return asset.referencePose.has_value();})||
        std::any_of(graph.states.begin(),graph.states.end(),[](const auto &state){return state.referencePose.has_value();});
    const bool orderedReference=std::any_of(graph.assets.begin(),graph.assets.end(),[](const auto &asset){return asset.referencePose&&!asset.referencePose->order.empty();})||
        std::any_of(graph.states.begin(),graph.states.end(),[](const auto &state){return state.referencePose&&!state.referencePose->order.empty();});
    const auto hasGroupAim=[](const auto &reference){return reference&&std::any_of(reference->directions.begin(),reference->directions.end(),
        [](const auto &aim){return !aim.additionalDescendants.empty();});};
    const bool groupAim=std::any_of(graph.assets.begin(),graph.assets.end(),[&](const auto &asset){return hasGroupAim(asset.referencePose);})||
        std::any_of(graph.states.begin(),graph.states.end(),[&](const auto &state){return hasGroupAim(state.referencePose);});
    const bool rigSamples=std::any_of(graph.assets.begin(),graph.assets.end(),[](const auto &asset){return !asset.rigSampleCatalogs.empty();});
    const bool uvSets=std::any_of(graph.assets.begin(),graph.assets.end(),[](const auto &asset)
    {
        return std::any_of(asset.materials.begin(),asset.materials.end(),[](const auto &material){return !material.baseColorUvSet.empty();})||
               std::any_of(asset.meshes.begin(),asset.meshes.end(),[](const auto &mesh)
        {return mesh.topology?(!mesh.topology->primaryUvName.empty()||!mesh.topology->additionalUvSets.empty()):
                              (!mesh.primaryUvName.empty()||!mesh.additionalUvSets.empty());});
    })||std::any_of(graph.adjustments.begin(),graph.adjustments.end(),[](const auto &adjustment)
    {
        if(adjustment.materials&&std::any_of(adjustment.materials->begin(),adjustment.materials->end(),
            [](const auto &material){return !material.baseColorUvSet.empty();}))return true;
        return std::any_of(adjustment.meshes.begin(),adjustment.meshes.end(),
            [](const auto &mesh){return !mesh.additionalUv.empty();});
    });
    const auto hasNormal=[](const auto &materials)
    {return std::any_of(materials.begin(),materials.end(),[](const auto &m){return m.normalImage||!m.normalUvSet.empty()||m.normalScale!=1;});};
    const bool normalMaps=std::any_of(graph.assets.begin(),graph.assets.end(),[&](const auto &asset){return hasNormal(asset.materials);})||
        std::any_of(graph.adjustments.begin(),graph.adjustments.end(),[&](const auto &a){return a.materials&&hasNormal(*a.materials);});
    const bool normalEdits=std::any_of(graph.adjustments.begin(),graph.adjustments.end(),[](const auto &a)
        {return std::any_of(a.meshes.begin(),a.meshes.end(),[](const auto &m){return !m.normals.empty();});});
    const bool sourceTangents=std::any_of(graph.assets.begin(),graph.assets.end(),[](const auto &asset)
        {return std::any_of(asset.meshes.begin(),asset.meshes.end(),[](const auto &mesh){return mesh.topology&&mesh.topology->sourceTangents;});});
    const bool glbTangents=std::any_of(graph.assets.begin(),graph.assets.end(),[](const auto &asset)
        {return std::any_of(asset.meshes.begin(),asset.meshes.end(),[](const auto &mesh){return mesh.topology&&mesh.topology->sourceGlbTangents;});});
    const bool authoredSkinning=std::any_of(graph.assets.begin(),graph.assets.end(),[](const auto &asset)
        {return std::any_of(asset.meshes.begin(),asset.meshes.end(),[](const auto &mesh)
            {return (mesh.topology?mesh.topology->skinning:mesh.geometry.skinning)!=SkinningProfile{};});})||
        std::any_of(graph.adjustments.begin(),graph.adjustments.end(),[](const auto &adjustment)
            {return std::any_of(adjustment.meshes.begin(),adjustment.meshes.end(),[](const auto &mesh){return mesh.skinning.has_value();});});
    const bool morphDefaults=std::any_of(graph.assets.begin(),graph.assets.end(),[](const auto &asset)
        {return std::any_of(asset.meshes.begin(),asset.meshes.end(),[](const auto &mesh){return !mesh.morphDefaults.empty();});});
    const bool surfaceBasis=std::any_of(graph.programs.begin(),graph.programs.end(),[](const auto &p)
        {return !p.definition.surfaceOffsetBases.empty();});
    const bool groundingSupport=std::any_of(graph.programs.begin(),graph.programs.end(),[](const auto &p)
        {return p.definition.grounding&&!p.definition.grounding->supportPoints.empty();});
    const bool selectionRemap=std::any_of(graph.history.begin(),graph.history.end(),[](const auto &entry)
        {return !entry.selectionRemap.empty();});
    bool selectionInsertion=false,selectionMixed=false;
    for(std::size_t i=1;i<graph.history.size();++i)if(!graph.history[i].selectionRemap.empty())
    {
        const auto &mapping=graph.history[i].selectionRemap;
        const auto mapped=static_cast<std::size_t>(std::count_if(mapping.begin(),mapping.end(),[](const auto &v){return v.has_value();}));
        const bool removed=mapped!=mapping.size(),added=mapped<graph.assets.at(graph.history[i].asset).meshes.size();
        selectionMixed|=removed&&added;selectionInsertion|=!removed&&added;
    }
    bool selectionTransfer=false;
    for(std::size_t i=1;i<graph.history.size();++i)
        if(graph.history[i].asset!=graph.history[i-1].asset&&graph.history[i].selections!=graph.history[i-1].selections)
            selectionTransfer=true;
    const auto hasMaterialSurface=[](const auto &materials)
        {return std::any_of(materials.begin(),materials.end(),[](const auto &m){return m.surface!=Object3DMaterialSurface{};});};
    const bool materialSurface=std::any_of(graph.assets.begin(),graph.assets.end(),[&](const auto &a){return hasMaterialSurface(a.materials);})||
        std::any_of(graph.adjustments.begin(),graph.adjustments.end(),[&](const auto &a){return a.materials&&hasMaterialSurface(*a.materials);});
    const std::uint32_t version=selectionMixed?35:selectionInsertion?34:selectionRemap?33:groundingSupport?32:!graph.progressiveMorphPrograms.empty()?31:groupAim?30:materialSurface?29:selectionTransfer?28:!graph.animations.empty()?27:!graph.correctivePrograms.empty()?26:surfaceBasis?25:glbTangents?24:morphDefaults?23:authoredSkinning?22:!graph.morphPrograms.empty()?21:sourceTangents?20:normalEdits?19:normalMaps?18:!graph.selections.empty()?17:uvSets?16:orderedReference?15:rigSamples?14:referencePose?13:guardedFairing?12:fairing?11:evaluatedLayers?10:std::any_of(graph.programs.begin(),graph.programs.end(),[](const auto &p){return !p.definition.fittings.empty();})?9:8;
    return version;
}
}
