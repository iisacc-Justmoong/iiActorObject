#include "HumanAccessories.h"
#include "HumanAccessoryCatalogInternal.h"
#include <array>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace iiActorObject::NSCharacter
{
using human_detail::Files;
using human_detail::accessoryCatalog;
std::vector<HumanAccessoryInfo> listMakeHumanAccessories(const std::filesystem::path &directory,std::size_t maximumBytes)
{
    Files files(directory,maximumBytes);const auto entries=accessoryCatalog(files);std::vector<HumanAccessoryInfo> result;result.reserve(entries.size());
    for(const auto &entry:entries)result.push_back(entry.info);return result;
}
bool setHumanAccessories(CharacterEditor &editor,const CharacterTemplate &alternative,
    const std::filesystem::path &directory,std::size_t maximumCatalogueBytes)
{return setHumanAccessories(editor,alternative,directory,TemplateSurfaceRemovalOptions{},maximumCatalogueBytes);}
bool setHumanAccessories(CharacterEditor &editor,const CharacterTemplate &alternative,
    const std::filesystem::path &directory,const TemplateSurfaceRemovalOptions &removal,std::size_t maximumCatalogueBytes)
{
    const auto current=editor.session().get().templateProgram();
    if(!current.valid()||!alternative.valid())throw std::invalid_argument("Human accessory changes require live templates");
    const auto &before=current.get().definition().base.get().data();
    const auto &after=alternative.program().get().definition().base.get().data();
    if(before.meshes.empty()||after.meshes.empty())throw std::invalid_argument("Human accessory changes require an existing body");
    const auto catalogue=listMakeHumanAccessories(directory,maximumCatalogueBytes);
    std::map<std::string_view,std::string_view,std::less<>> categories;
    for(const auto &entry:catalogue)categories.emplace(entry.name,entry.category);
    std::set<std::string_view,std::less<>> oldNames,newNames;
    for(const auto &mesh:before.meshes)if(!oldNames.insert(mesh.name).second)throw std::invalid_argument("Human accessory mesh names must be unique");
    for(const auto &mesh:after.meshes)if(!newNames.insert(mesh.name).second)throw std::invalid_argument("Human accessory mesh names must be unique");
    std::vector<std::size_t> removed,added;std::vector<std::string_view> oldRetained,newRetained;
    for(std::size_t i=0;i<before.meshes.size();++i)
    {
        const auto &name=before.meshes[i].name;
        if(newNames.contains(name))oldRetained.push_back(name);
        else if(i==0||!categories.contains(name))throw std::invalid_argument("Only existing human accessory slots can be removed");
        else removed.push_back(i);
    }
    for(std::size_t i=0;i<after.meshes.size();++i)
    {
        const auto &name=after.meshes[i].name;
        if(oldNames.contains(name))newRetained.push_back(name);
        else if(i==0||!categories.contains(name))throw std::invalid_argument("Only catalogued human accessory slots can be added");
        else added.push_back(i);
    }
    if(oldRetained!=newRetained)throw std::invalid_argument("Retained human accessory slots must keep their relative order");
    if(!removed.empty()&&!added.empty())
    {
        bool directReplacement=before.meshes.size()==after.meshes.size();
        if(directReplacement)for(std::size_t i=0;i<before.meshes.size();++i)
        {
            const auto &old=before.meshes[i].name,&replacement=after.meshes[i].name;
            if(old==replacement)continue;
            const auto a=categories.find(old),b=categories.find(replacement);
            if(i==0||newNames.contains(old)||oldNames.contains(replacement)||
               a==categories.end()||b==categories.end()||a->second!=b->second)
            {directReplacement=false;break;}
        }
        if(!directReplacement)
            return editor.session().get().changeTemplateSurfaces(alternative.program(),removed,added,std::array<std::size_t,1>{0},removal);
    }
    if(before.meshes.size()<after.meshes.size())
    {
        std::vector<std::size_t> added;std::size_t source=0;
        for(std::size_t target=0;target<after.meshes.size();++target)
        {
            if(source<before.meshes.size()&&before.meshes[source].name==after.meshes[target].name){++source;continue;}
            if(source==0||!categories.contains(after.meshes[target].name))
                throw std::invalid_argument("Only catalogued human accessory slots can be added");
            added.push_back(target);
        }
        if(source!=before.meshes.size())throw std::invalid_argument("Human accessory addition must retain other slots in order");
        return editor.session().get().addTemplateSurfaces(alternative.program(),added,std::array<std::size_t,1>{0});
    }
    if(before.meshes.size()>after.meshes.size())
    {
        std::vector<std::size_t> removed;std::size_t target=0;
        for(std::size_t source=0;source<before.meshes.size();++source)
        {
            if(target<after.meshes.size()&&before.meshes[source].name==after.meshes[target].name){++target;continue;}
            if(source==0||!categories.contains(before.meshes[source].name))
                throw std::invalid_argument("Only existing human accessory slots can be removed");
            removed.push_back(source);
        }
        if(target!=after.meshes.size())throw std::invalid_argument("Human accessory removal must retain other slots in order");
        return editor.session().get().removeTemplateSurfaces(alternative.program(),removed,std::array<std::size_t,1>{0},removal);
    }
    std::vector<std::size_t> changed;
    for(std::size_t i=0;i<before.meshes.size();++i)
    {
        const auto &old=before.meshes[i].name,&replacement=after.meshes[i].name;
        if(old==replacement)continue;
        const auto a=categories.find(old),b=categories.find(replacement);
        if(i==0||a==categories.end()||b==categories.end()||a->second!=b->second)
            throw std::invalid_argument("Human accessory slots must keep their catalogue category");
        changed.push_back(i);
    }
    return editor.session().get().replaceTemplateSurfaces(alternative.program(),changed,std::array<std::size_t,1>{0});
}
} // namespace iiActorObject::NSCharacter
