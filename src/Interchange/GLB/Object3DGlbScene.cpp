#include "Object3DGlbScene.h"
#include "Object3DGlbImportOptions.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace iiActorObject::object3d_detail::glb_import
{
Scene readScene(const Value& root,const GlbImportOptions& options,std::vector<std::string>& warnings)
{
    const auto& values=list(root,"nodes");if(values.empty()||values.size()>options.maximumNodes)fail("invalid node count");
    Scene scene;scene.nodes.resize(values.size());std::vector<std::size_t> globalParents(values.size(),NoBone);
    for(std::size_t i=0;i<values.size();++i)
    {
        scene.nodes[i].source=&values[i];(void)object(values[i]);
        for(const auto& child:list(values[i],"children"))
        {const auto id=integer(child);if(id>=values.size()||id==i||globalParents[id]!=NoBone)fail("invalid or multiple node parents");globalParents[id]=i;}
    }
    std::vector<std::size_t> roots;const auto& scenes=list(root,"scenes");
    if(!scenes.empty())
    {
        const auto selected=optionalInteger(root,"scene");if(selected>=scenes.size())fail("invalid scene index");
        for(const auto& node:list(scenes[selected],"nodes"))roots.push_back(integer(node));
        if(scenes.size()>1)warnings.emplace_back("Only the selected GLB scene was imported");
    }
    else {if(field(root,"scene"))fail("scene index without scenes");for(std::size_t i=0;i<values.size();++i)if(globalParents[i]==NoBone)roots.push_back(i);warnings.emplace_back("GLB has no scenes; parentless nodes were used");}
    for(const auto id:roots)
    {if(id>=values.size()||scene.nodes[id].active)fail("invalid or duplicate scene root");scene.nodes[id].active=true;scene.nodes[id].world=localMatrix(values[id]);scene.order.push_back(id);}
    for(std::size_t cursor=0;cursor<scene.order.size();++cursor)
    {
        const auto id=scene.order[cursor];auto& n=scene.nodes[id];extensions(*n.source,options,warnings);
        if(field(*n.source,"mesh"))scene.meshNodes.push_back(id);
        if(field(*n.source,"camera"))unsupported(options,warnings,"GLB camera node discarded");
        for(const auto& child:list(*n.source,"children"))
        {
            const auto next=integer(child);auto& target=scene.nodes[next];if(target.active)fail("node cycle or duplicate scene traversal");
            target.active=true;target.parent=id;const auto local=localMatrix(values[next]);target.world=ufbx_matrix_mul(&n.world,&local);scene.order.push_back(next);
        }
    }
    if(scene.meshNodes.empty()||scene.meshNodes.size()>options.maximumMeshes)fail("invalid mesh instance count");
    return scene;
}
} // namespace iiActorObject::object3d_detail::glb_import
