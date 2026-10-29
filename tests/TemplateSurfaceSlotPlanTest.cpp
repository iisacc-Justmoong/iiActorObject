#include "../src/Geometry/Object3DTemplateSurfaceSlots.h"
#include <iiActorObject/Skeleton.h>
#include <array>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace iiActorObject;
using namespace iiActorObject::object3d_detail;
namespace
{
void check(bool value,const std::source_location &where=std::source_location::current())
{if(!value)throw std::runtime_error("Template surface slot contract failed at line "+std::to_string(where.line()));}
template<class Error=std::invalid_argument,class F>void rejects(F operation,std::string_view message)
{
    try{operation();}
    catch(const Error &error)
    {if(error.what()!=message)throw std::runtime_error("Expected: "+std::string(message)+"; received: "+error.what());return;}
    throw std::runtime_error("Invalid template surface slots accepted");
}
}
int main()
{
    const std::vector<Bone> bones{{"root",NoBone,{}},{"child",0,{}}};
    const std::array<std::size_t,2> removed{1,3},added{0,3};
    const std::array<std::size_t,1> visible{2};
    const auto change=planTemplateSurfaceSlots(TemplateSurfaceOperation::Change,4,4,bones,bones,removed,added,visible);
    check(change.sourceSlots==std::set<std::size_t>({1,3})&&change.addedSlots==std::set<std::size_t>({0,3}));
    check(change.visibility==std::set<std::size_t>({2}));
    check(change.oldToNew==std::vector<std::optional<std::size_t>>({1,std::nullopt,2,std::nullopt}));
    check(change.newToOld==std::vector<std::optional<std::size_t>>({std::nullopt,0,2,std::nullopt}));
    const auto removal=planTemplateSurfaceSlots(TemplateSurfaceOperation::Remove,4,2,bones,bones,removed,{},visible);
    check(removal.oldToNew==std::vector<std::optional<std::size_t>>({0,std::nullopt,1,std::nullopt}));
    check(removal.newToOld==std::vector<std::optional<std::size_t>>({0,2}));
    const auto replace=planTemplateSurfaceSlots(TemplateSurfaceOperation::Replace,4,4,bones,bones,
        std::array<std::size_t,1>{1},{},visible);
    check(replace.sourceSlots.contains(1)&&replace.visibility.contains(2)&&replace.oldToNew.empty()&&replace.newToOld.empty());
    auto movedBones=bones;movedBones[1].bindTransform.position.x=2;
    (void)planTemplateSurfaceSlots(TemplateSurfaceOperation::Replace,4,4,bones,movedBones,{}, {},{});
    const auto unchanged=planTemplateSurfaceSlots(TemplateSurfaceOperation::Change,0,0,{}, {},{}, {},{});
    check(unchanged.oldToNew.empty()&&unchanged.newToOld.empty());

    const std::array<std::size_t,2> duplicate{1,1};
    rejects([&]{(void)planTemplateSurfaceSlots(TemplateSurfaceOperation::Change,4,4,bones,bones,removed,duplicate,visible);},
        "Invalid or duplicate added template surface");
    rejects([&]{(void)planTemplateSurfaceSlots(TemplateSurfaceOperation::Remove,4,2,bones,bones,duplicate,{},visible);},
        "Invalid or duplicate removed template surface");
    rejects([&]{(void)planTemplateSurfaceSlots(TemplateSurfaceOperation::Replace,4,4,bones,bones,
        std::array<std::size_t,1>{4},{},{});},"Invalid or duplicate template surface slot");
    rejects([&]{(void)planTemplateSurfaceSlots(TemplateSurfaceOperation::Change,4,4,bones,bones,removed,added,
        std::array<std::size_t,1>{1});},"Invalid or duplicate template visibility slot");
    rejects([&]{(void)planTemplateSurfaceSlots(TemplateSurfaceOperation::Replace,4,4,bones,bones,
        std::array<std::size_t,1>{1},{},std::array<std::size_t,2>{2,2});},"Invalid or duplicate template visibility slot");
    rejects([&]{(void)planTemplateSurfaceSlots(TemplateSurfaceOperation::Change,4,5,bones,bones,removed,added,{});},
        "Template change requires retained mesh slots and skeleton");
    rejects([&]{(void)planTemplateSurfaceSlots(TemplateSurfaceOperation::Remove,4,3,bones,bones,removed,{},{});},
        "Template removal requires retained mesh slots and skeleton");
    rejects([&]{(void)planTemplateSurfaceSlots(TemplateSurfaceOperation::Replace,4,3,bones,bones,{}, {},{});},
        "Template surface replacement requires stable mesh slots and skeleton");
    auto other=bones;other[1].name="other";
    rejects([&]{(void)planTemplateSurfaceSlots(TemplateSurfaceOperation::Change,4,4,bones,other,removed,added,{});},
        "Template addition skeleton identities changed");
    other=bones;other[1].parent=NoBone;
    rejects([&]{(void)planTemplateSurfaceSlots(TemplateSurfaceOperation::Remove,4,2,bones,other,removed,{},{});},
        "Template removal skeleton identities changed");
    rejects([&]{(void)planTemplateSurfaceSlots(TemplateSurfaceOperation::Replace,4,4,bones,other,{}, {},{});},
        "Template surface replacement skeleton identities changed");
    rejects([&]{(void)planTemplateSurfaceSlots(TemplateSurfaceOperation::Change,4,4,bones,std::span(bones).first(1),removed,added,{});},
        "Template change requires retained mesh slots and skeleton");
    rejects([&]{(void)planTemplateSurfaceSlots(TemplateSurfaceOperation::Remove,4,2,bones,bones,removed,added,{});},
        "Unexpected added template surfaces");
    rejects([&]{(void)planTemplateSurfaceSlots(static_cast<TemplateSurfaceOperation>(-1),0,0,{}, {},{}, {},{});},
        "Unknown template surface operation");
    const std::vector<std::size_t> tooMany(33);
    rejects<std::length_error>([&]{(void)planTemplateSurfaceSlots(TemplateSurfaceOperation::Change,4,4,bones,bones,{},tooMany,{});},
        "Template surface change limit");
    rejects<std::length_error>([&]{(void)planTemplateSurfaceSlots(TemplateSurfaceOperation::Remove,4,4,bones,bones,tooMany,{},{});},
        "Template surface removal limit");
    rejects<std::length_error>([&]{(void)planTemplateSurfaceSlots(TemplateSurfaceOperation::Replace,4,4,bones,bones,tooMany,{},{});},
        "Template surface replacement limit");
    rejects([&]{(void)planTemplateSurfaceSlots(TemplateSurfaceOperation::Change,
        std::numeric_limits<std::size_t>::max(),1,{}, {},{},std::array<std::size_t,1>{0},{});},
        "Template change requires retained mesh slots and skeleton");

    // Exhaust all small old/new selections and check both inverse and stable order.
    for(std::size_t oldCount=0;oldCount<=5;++oldCount)for(std::size_t newCount=0;newCount<=5;++newCount)
        for(unsigned removedBits=0;removedBits<(1u<<oldCount);++removedBits)
            for(unsigned addedBits=0;addedBits<(1u<<newCount);++addedBits)
            {
                std::vector<std::size_t> oldSlots,newSlots;
                for(std::size_t i=0;i<oldCount;++i)if(removedBits&(1u<<i))oldSlots.push_back(i);
                for(std::size_t i=0;i<newCount;++i)if(addedBits&(1u<<i))newSlots.push_back(i);
                if(oldCount-oldSlots.size()!=newCount-newSlots.size())continue;
                const auto plan=planTemplateSurfaceSlots(TemplateSurfaceOperation::Change,oldCount,newCount,{}, {},oldSlots,newSlots,{});
                std::optional<std::size_t> previous;
                for(std::size_t i=0;i<oldCount;++i)
                {
                    if(removedBits&(1u<<i)){check(!plan.oldToNew[i]);continue;}
                    check(plan.oldToNew[i].has_value());const auto next=*plan.oldToNew[i];
                    check(next<newCount&&!(addedBits&(1u<<next))&&plan.newToOld[next]==i);
                    check(!previous||*previous<next);previous=next;
                }
                for(std::size_t i=0;i<newCount;++i)
                    check(static_cast<bool>(plan.newToOld[i])==!(addedBits&(1u<<i)));
            }
}
