#include "Object3DProjectRigRestore.h"
#include "Object3DProjectArchive.h"
#include "Object3DProjectOptions.h"
#include "Object3DPoseRigBackend.h"
#include "Object3DTemplateMorphBackend.h"
#include "Object3DProgressiveMorphBackend.h"
#include "Object3DPoseCorrectivesBackend.h"
#include "Object3DDnaBackend.h"
#include <algorithm>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail
{
std::vector<Object3DRigHandle> restoreProjectRigs(const Object3DReference &engine,
    const ProjectArchive &graph,const std::vector<Object3DHandle> &assets,
    const std::vector<Object3DTemplateHandle> &programs,const Object3DProjectOptions &options)
{
    std::vector<Object3DRigHandle> rigs;
    DnaImportOptions dnaOptions;dnaOptions.maximumBytes=options.maximumBytes;dnaOptions.maximumMemory=options.maximumDnaMemory;
    dnaOptions.maximumJoints=std::min<std::size_t>(65535,options.maximumElements);dnaOptions.maximumMeshes=std::min<std::size_t>(65535,options.maximumElements);
    std::vector<std::unique_ptr<Object3DRigDriver>> drivers;for(const auto &bytes:graph.dna)drivers.push_back(makeDnaProgramDriver(std::as_bytes(std::span(bytes)),dnaOptions));
    std::vector<std::unique_ptr<Object3DRigDriver>> poseDrivers;
    for(const auto &bytes:graph.posePrograms)poseDrivers.push_back(makePoseProgramDriver(importObject3DPoseProgram(std::as_bytes(std::span(bytes)),options.maximumBytes)));
    std::vector<std::unique_ptr<Object3DRigDriver>> correctiveDrivers;
    for(const auto &bytes:graph.correctivePrograms)correctiveDrivers.push_back(makePoseCorrectiveDriver(
        importObject3DPoseCorrectiveProgram(std::as_bytes(std::span(bytes)),options.maximumBytes)));
    std::vector<bool> usedCorrectives(graph.correctivePrograms.size(),false);
    std::vector<bool> usedMorphPrograms(graph.morphPrograms.size(),false);
    std::vector<bool> usedProgressivePrograms(graph.progressiveMorphPrograms.size(),false);
    std::map<std::pair<std::size_t,std::optional<std::size_t>>,std::unique_ptr<Object3DRigDriver>> morphDrivers;
    for(const auto &rig:graph.rigs)
    {
        const auto object=assets.at(rig.asset);
        if(rig.kind==2)
        {
            const auto &stored=graph.morphPrograms.at(rig.source);usedMorphPrograms[rig.source]=true;
            const auto key=std::pair{rig.source,rig.templateProgram};auto found=morphDrivers.find(key);
            if(found==morphDrivers.end())
            {
                const auto source=rig.templateProgram?programs.at(*rig.templateProgram):Object3DTemplateHandle{};
                found=morphDrivers.emplace(key,makeTemplateMorphRigDriver({source,stored})).first;
            }
            rigs.push_back(engine.createRigSession(object,found->second->cloneFor({&object.get(),rig.joints}),rig.joints));
        }
        else if(rig.kind==4)
        {
            const auto &stored=graph.progressiveMorphPrograms.at(rig.source);usedProgressivePrograms[rig.source]=true;
            auto source=makeProgressiveMorphRigDriver(stored,object.get());
            rigs.push_back(engine.createRigSession(object,std::move(source),rig.joints));
        }
        else if(rig.kind==3)
        {
            auto &source=correctiveDrivers.at(rig.source);usedCorrectives[rig.source]=true;
            rigs.push_back(engine.createRigSession(object,source->cloneFor({&object.get(),rig.joints}),rig.joints));
        }
        else
        {
            auto &source=rig.kind==1?poseDrivers.at(rig.source):drivers.at(rig.source);
            rigs.push_back(engine.createRigSession(object,source->cloneFor({&object.get(),rig.joints}),rig.joints));
        }
    }
    if(std::any_of(usedMorphPrograms.begin(),usedMorphPrograms.end(),[](bool value){return !value;}))throw std::invalid_argument("Unreferenced project template morph program");
    if(std::any_of(usedProgressivePrograms.begin(),usedProgressivePrograms.end(),[](bool value){return !value;}))throw std::invalid_argument("Unreferenced project progressive morph program");
    if(std::any_of(usedCorrectives.begin(),usedCorrectives.end(),[](bool value){return !value;}))throw std::invalid_argument("Unreferenced project pose corrective program");
    return rigs;
}
} // namespace iiActorObject::object3d_detail
