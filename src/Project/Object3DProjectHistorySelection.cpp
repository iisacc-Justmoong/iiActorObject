#include "Object3DProjectHistorySelection.h"
#include "Object3DSelectionValidation.h"
#include "Object3DData.h"
#include <algorithm>
#include <stdexcept>
namespace iiActorObject::object3d_detail
{
void validateProjectHistorySelectionTransition(std::uint32_t version,
    const ProjectHistorySelectionView *previous,const ProjectHistorySelectionView &current,
    bool sameObject,std::span<const std::optional<std::size_t>> remapping)
{
    if(!remapping.empty())
    {
        if(!previous||version<33||!current.selection||!previous->selection||!current.layered||!previous->layered)
            throw std::invalid_argument("Invalid project selection remapping transition");
        const auto mapped=std::count_if(remapping.begin(),remapping.end(),[](const auto &value){return value.has_value();});
        const bool removed=static_cast<std::size_t>(mapped)!=remapping.size();
        const bool added=static_cast<std::size_t>(mapped)<current.object.meshes.size();
        const auto mode=removed&&added?SelectionRemapMode::Mixed:added?SelectionRemapMode::Insertion:SelectionRemapMode::Removal;
        if(mode==SelectionRemapMode::Mixed&&version<35)throw std::invalid_argument("Mixed project selection remapping requires version 35");
        if(mode==SelectionRemapMode::Insertion&&version<34)throw std::invalid_argument("Project selection insertion requires version 34");
        validateRemappedPointSelectionTransition(previous->object,current.object,
            *previous->selection,*current.selection,remapping,mode);
    }
    else if(previous)
    {
        if(current.selection!=previous->selection)
        {
            if(!sameObject)
            {
                if(version<28||!current.selection||!previous->selection||current.layered||
                   current.selection->entries.size()!=previous->selection->entries.size())
                    throw std::invalid_argument("Project selection change also changes the object");
                for(std::size_t i=0;i<current.selection->entries.size();++i)
                    if(current.selection->entries[i].name!=previous->selection->entries[i].name)
                        throw std::invalid_argument("Project transferred selection names differ");
            }
        }
        else if(current.selection&&!sameObject)
            validatePointSelectionTransition(previous->object,current.object,*current.selection);
    }
}
}
