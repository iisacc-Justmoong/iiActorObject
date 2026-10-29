#include "Object3DAdjustment.h"
#include "Object3DSelectionState.h"
#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <stdexcept>

namespace iiActorObject
{
namespace
{
bool sameMaterial(const Object3DMaterial &a,const Object3DMaterial &b)
{
    return a.name==b.name&&a.baseColor==b.baseColor&&a.metallic==b.metallic&&a.roughness==b.roughness&&
        a.baseColorImage==b.baseColorImage&&a.baseColorUvSet==b.baseColorUvSet&&a.normalImage==b.normalImage&&
        a.normalUvSet==b.normalUvSet&&a.normalScale==b.normalScale&&a.surface==b.surface;
}
bool usesMaterial(const Object3DMesh &mesh,std::size_t material)
{
    if(mesh.material==material)return true;
    if(mesh.topology)
    {
        const auto &values=mesh.topology->layout->definition().polygonMaterials;
        return std::find(values.begin(),values.end(),material)!=values.end();
    }
    return std::find(mesh.triangleMaterials.begin(),mesh.triangleMaterials.end(),material)!=mesh.triangleMaterials.end();
}
bool usesImage(const Object3DMaterial &material,std::size_t image)
{return material.baseColorImage==image||material.normalImage==image;}
void singleMaterial(const Object3DMesh &mesh)
{
    if(mesh.topology)
    {
        const auto &values=mesh.topology->layout->definition().polygonMaterials;
        if(std::any_of(values.begin(),values.end(),[&](auto value){return value!=mesh.material;}))
            throw std::invalid_argument("Template surface replacement requires one material per replaced mesh");
    }
    else if(std::any_of(mesh.triangleMaterials.begin(),mesh.triangleMaterials.end(),[&](auto value){return value!=mesh.material;}))
        throw std::invalid_argument("Template surface replacement requires one material per replaced mesh");
}
void visibilityLayout(const MeshTopology &old,const MeshTopology &next,bool ignoreMaterials=false)
{
    if(!old.layout||!next.layout||old.points.size()!=next.points.size())throw std::invalid_argument("Template visibility source points changed");
    auto left=old.layout->definition(),right=next.layout->definition();
    left.hiddenPolygons.clear();right.hiddenPolygons.clear();left.renderToLayout.clear();right.renderToLayout.clear();
    if(ignoreMaterials){left.polygonMaterials.clear();right.polygonMaterials.clear();}
    if(left!=right)throw std::invalid_argument("Template visibility source topology changed");
    constexpr std::size_t limit=2'000'000;
    if(left.polygons.size()>limit||old.layout->trianglePolygons().size()>limit||next.layout->trianglePolygons().size()>limit)
        throw std::length_error("Template visibility comparison budget");
    using Triangle=std::array<std::uint32_t,3>;
    const auto collect=[&](const MeshTopologyLayout &layout)
    {
        std::vector<std::vector<Triangle>> faces(left.polygons.size());
        const auto triangles=layout.triangles(),polygons=layout.trianglePolygons();const auto &render=layout.definition().renderToLayout;
        for(std::size_t i=0;i<polygons.size();++i)
            faces[polygons[i]].push_back({render[triangles[3*i]],render[triangles[3*i+1]],render[triangles[3*i+2]]});
        return faces;
    };
    const auto previous=collect(*old.layout),replacement=collect(*next.layout);
    for(std::size_t i=0;i<previous.size();++i)
        if(!previous[i].empty()&&!replacement[i].empty()&&previous[i]!=replacement[i])
            throw std::invalid_argument("Template visibility changes existing triangulation");
}
void retainedLayout(const Object3DMesh &old,const Object3DMesh &next)
{
    if(old.name!=next.name||old.topology.has_value()!=next.topology.has_value())
        throw std::invalid_argument("Retained template mesh identity changed");
    if(!old.topology)
    {
        if(old.geometry.vertices.size()!=next.geometry.vertices.size()||old.geometry.triangles!=next.geometry.triangles)
            throw std::invalid_argument("Retained template render topology changed");
        return;
    }
    if(!old.topology->layout||!next.topology->layout||old.topology->points.size()!=next.topology->points.size())
        throw std::invalid_argument("Retained template source points changed");
    auto a=old.topology->layout->definition(),b=next.topology->layout->definition();
    a.polygonMaterials.clear();b.polygonMaterials.clear();
    const auto at=old.topology->layout->triangles(),bt=next.topology->layout->triangles();
    const auto af=old.topology->layout->trianglePolygons(),bf=next.topology->layout->trianglePolygons();
    if(a!=b||!std::equal(at.begin(),at.end(),bt.begin(),bt.end())||!std::equal(af.begin(),af.end(),bf.begin(),bf.end()))
        throw std::invalid_argument("Retained template source topology changed");
}
// Freeze the original visible support points before changing a grounded
// surface's visibility. Subsequent replacements retain the same support set.
void preserveGroundingSupport(Object3DTemplateDefinition &definition,const MeshTopology &old,const MeshTopology &next)
{
    auto &grounding=*definition.grounding;
    if(!grounding.supportPoints.empty())return;
    const auto count=old.points.size();
    if(count>2'000'000)throw std::length_error("Template grounding comparison budget");
    std::vector<bool> previous(count),replacement(count);
    const auto mark=[](const MeshTopology &topology,std::vector<bool> &selected)
    {
        const auto &layout=topology.layout->definition();
        for(const auto index:layout.renderToLayout)selected.at(layout.layouts.at(index).position)=true;
    };
    mark(old,previous);mark(next,replacement);
    if(previous==replacement)return;
    for(std::size_t i=0;i<count;++i)if(previous[i])grounding.supportPoints.push_back(static_cast<std::uint32_t>(i));
    if(grounding.supportPoints.empty())throw std::invalid_argument("Template grounding support is empty");
}
// Triangle-frame bindings use render indices, so match their original source
// polygon and layout corners after a visibility change. Control-point bindings
// already use stable source IDs and need no remapping.
void remapVisibilityBinding(SurfaceBinding &binding,const MeshTopology &old,const MeshTopology &next)
{
    if(binding.mode!=SurfaceBindingMode::TriangleFrame)return;
    using Key=std::array<std::uint32_t,4>;
    using Triangle=std::array<std::uint32_t,3>;
    const auto &oldLayout=*old.layout,&newLayout=*next.layout;
    const auto oldTriangles=oldLayout.triangles(),oldFaces=oldLayout.trianglePolygons();
    const auto newTriangles=newLayout.triangles(),newFaces=newLayout.trianglePolygons();
    const auto &oldRender=oldLayout.definition().renderToLayout,&newRender=newLayout.definition().renderToLayout;
    if(binding.sourceVertexCount!=oldRender.size())throw std::invalid_argument("Template follower source identity changed");
    const auto key=[](std::uint32_t face,std::span<const std::uint32_t> triangles,
        std::span<const std::uint32_t> render,std::size_t index)
    {return Key{face,render[triangles[index*3]],render[triangles[index*3+1]],render[triangles[index*3+2]]};};
    std::map<Key,std::pair<std::uint32_t,Triangle>> surviving;
    for(std::size_t i=0;i<newFaces.size();++i)
    {
        if(i>=NoTopologyIndex)throw std::length_error("Template follower triangle limit");
        const auto k=key(newFaces[i],newTriangles,newRender,i);
        surviving.try_emplace(k,std::pair{static_cast<std::uint32_t>(i),Triangle{newTriangles[3*i],newTriangles[3*i+1],newTriangles[3*i+2]}});
    }
    for(auto &point:binding.points)
    {
        if(point.triangle>=oldFaces.size())throw std::invalid_argument("Template follower triangle is invalid");
        const auto match=surviving.find(key(oldFaces[point.triangle],oldTriangles,oldRender,point.triangle));
        if(match==surviving.end())throw std::invalid_argument("Template visibility hides a follower anchor triangle");
        point.triangle=match->second.first;point.vertices=match->second.second;
    }
    binding.sourceVertexCount=newRender.size();
}
std::size_t bindingIndex(const Object3DTemplateDefinition &definition,std::size_t target)
{
    for(std::size_t i=0;i<definition.surfaces.size();++i)if(definition.surfaces[i].targetMesh==target)return i;
    throw std::invalid_argument("Template surface replacement requires a follower binding");
}
std::optional<SurfaceBindingBasis> bindingBasis(const Object3DTemplateDefinition &definition,std::size_t index)
{return definition.surfaceOffsetBases.empty()?std::nullopt:definition.surfaceOffsetBases.at(index);}

// Resource snapshots are complete arrays and may have different lengths after
// a skin edit. Pad them to common slots before appending new resources so an
// older layer cannot accidentally replace a newly attached surface's images.
class AdditionResources final
{
  public:
    AdditionResources(Object3DData &base,const Object3DData &candidate,
        std::vector<std::shared_ptr<object3d_detail::Adjustment>> &layers)
        :base_(base),candidate_(candidate),layers_(layers)
    {
        std::size_t maxImages=base.images.size(),maxMaterials=base.materials.size();
        for(const auto &image:base.images)imageNames_.insert(std::string(image.name()));
        for(const auto &material:base.materials)materialNames_.insert(material.name);
        for(const auto &layer:layers_)if(layer)
        {
            if(layer->images){maxImages=std::max(maxImages,layer->images->size());for(const auto &image:*layer->images)imageNames_.insert(std::string(image.name()));}
            if(layer->materials){maxMaterials=std::max(maxMaterials,layer->materials->size());for(const auto &material:*layer->materials)materialNames_.insert(material.name);}
        }
        if(maxImages>4096||maxMaterials>4096)throw std::length_error("Template surface resource slot budget");
        std::optional<Object3DImage> sample;
        const auto consider=[&](const Object3DImage &image)
        {if(!sample||image.encoded().size()<sample->encoded().size())sample=image;};
        for(const auto &image:base.images)consider(image);
        for(const auto &image:candidate.images)consider(image);
        for(const auto &layer:layers_)if(layer&&layer->images)for(const auto &image:*layer->images)consider(image);
        std::vector<std::optional<Object3DImage>> imagePadding(maxImages);
        const auto padImages=[&](std::vector<Object3DImage> &images)
        {
            while(images.size()<maxImages)
            {
                const auto index=images.size();
                if(!imagePadding[index])
                {
                    if(!sample)throw std::logic_error("Missing template image padding source");
                    imagePadding[index]=copyImage(unique(imageNames_,"iiActorObject.PaddingImage"),*sample);
                }
                images.push_back(*imagePadding[index]);
            }
        };
        padImages(base_.images);for(const auto &layer:layers_)if(layer&&layer->images)padImages(*layer->images);
        std::vector<std::optional<Object3DMaterial>> materialPadding(maxMaterials);
        const auto padMaterials=[&](std::vector<Object3DMaterial> &materials)
        {
            while(materials.size()<maxMaterials)
            {
                const auto index=materials.size();
                if(!materialPadding[index]){Object3DMaterial pad;pad.name=unique(materialNames_,"iiActorObject.PaddingMaterial");materialPadding[index]=std::move(pad);}
                materials.push_back(*materialPadding[index]);
            }
        };
        padMaterials(base_.materials);for(const auto &layer:layers_)if(layer&&layer->materials)padMaterials(*layer->materials);
    }
    void linkMaterial(std::size_t candidate,std::size_t old)
    {
        if(candidate>=candidate_.materials.size()||old>=base_.materials.size())throw std::invalid_argument("Invalid retained material mapping");
        const auto [found,added]=materials_.emplace(candidate,old);
        if(!added&&found->second!=old)throw std::invalid_argument("Retained material sharing changed");
    }
    void linkRetainedImages()
    {
        for(const auto &[incoming,existing]:materials_)
        {
            const auto &a=candidate_.materials[incoming],&b=base_.materials[existing];
            linkImage(a.baseColorImage,b.baseColorImage);linkImage(a.normalImage,b.normalImage);
        }
    }
    std::size_t material(std::size_t index)
    {
        if(const auto found=materials_.find(index);found!=materials_.end())return found->second;
        auto value=candidate_.materials.at(index);
        if(value.baseColorImage)value.baseColorImage=image(*value.baseColorImage);
        if(value.normalImage)value.normalImage=image(*value.normalImage);
        for(std::size_t i=0;i<base_.materials.size();++i)if(sameMaterial(base_.materials[i],value))
        {
            bool shared=true;for(const auto &layer:layers_)if(layer&&layer->materials&&
                (i>=layer->materials->size()||!sameMaterial(layer->materials->at(i),base_.materials[i]))){shared=false;break;}
            if(shared){materials_.emplace(index,i);return i;}
        }
        if(base_.materials.size()>=4096)throw std::length_error("Template material slot budget");
        if(!materialNames_.insert(value.name).second)value.name=unique(materialNames_,"iiActorObject.AddedMaterial");
        const auto result=base_.materials.size();base_.materials.push_back(value);
        for(const auto &layer:layers_)if(layer&&layer->materials)layer->materials->push_back(value);
        materials_.emplace(index,result);return result;
    }
  private:
    static std::string unique(std::set<std::string> &names,std::string_view prefix)
    {
        for(std::size_t i=1;i<=1'000'000;++i)
        {auto name=std::string(prefix)+"."+std::to_string(i);if(names.insert(name).second)return name;}
        throw std::length_error("Template resource name budget");
    }
    static Object3DImage copyImage(std::string name,const Object3DImage &image)
    {return Object3DImage(std::move(name),std::vector<std::uint8_t>(image.encoded().begin(),image.encoded().end()),std::string(image.mimeType()));}
    void linkImage(std::optional<std::size_t> incoming,std::optional<std::size_t> existing)
    {
        if(!incoming)return;
        if(!existing){ambiguousImages_.insert(*incoming);return;}
        if(*incoming>=candidate_.images.size()||*existing>=base_.images.size())throw std::invalid_argument("Invalid retained image mapping");
        const auto [found,added]=images_.emplace(*incoming,*existing);
        if(!added&&found->second!=*existing)ambiguousImages_.insert(*incoming);
    }
    std::size_t image(std::size_t index)
    {
        if(ambiguousImages_.contains(index))throw std::invalid_argument("Retained image sharing changed");
        if(const auto found=images_.find(index);found!=images_.end())return found->second;
        const auto &value=candidate_.images.at(index);
        for(std::size_t i=0;i<base_.images.size();++i)if(base_.images[i]==value)
        {
            bool shared=true;for(const auto &layer:layers_)if(layer&&layer->images&&
                (i>=layer->images->size()||layer->images->at(i)!=base_.images[i])){shared=false;break;}
            if(shared){images_.emplace(index,i);return i;}
        }
        if(base_.images.size()>=4096)throw std::length_error("Template image slot budget");
        Object3DImage stored=value;
        if(!imageNames_.insert(std::string(value.name())).second)stored=copyImage(unique(imageNames_,"iiActorObject.AddedImage"),value);
        const auto result=base_.images.size();base_.images.push_back(stored);
        for(const auto &layer:layers_)if(layer&&layer->images)layer->images->push_back(stored);
        images_.emplace(index,result);return result;
    }
    Object3DData &base_;const Object3DData &candidate_;
    std::vector<std::shared_ptr<object3d_detail::Adjustment>> &layers_;
    std::set<std::string> materialNames_,imageNames_;
    std::map<std::size_t,std::size_t> materials_,images_;
    std::set<std::size_t> ambiguousImages_;
};
}

bool Object3DEditor::addTemplateSurfaces(const Object3DTemplateHandle &alternative,
    std::span<const std::size_t> meshes,std::span<const std::size_t> visibilityMeshes)
{
    return changeTemplateSurfaces(alternative,{},meshes,visibilityMeshes);
}
bool Object3DEditor::changeTemplateSurfaces(const Object3DTemplateHandle &alternative,
    std::span<const std::size_t> removedMeshes,std::span<const std::size_t> addedMeshes,
    std::span<const std::size_t> visibilityMeshes,const TemplateSurfaceRemovalOptions &options)
{
    if(addedMeshes.empty())return removeTemplateSurfaces(alternative,removedMeshes,visibilityMeshes,options);
    if(!layered())throw std::logic_error("Template surface addition requires a template editor");
    const auto &next=alternative.get().definition();
    if(!owner_.owns(next.base))throw std::invalid_argument("Template surface addition requires the same engine");
    auto state=copyLayers();auto definition=state->program.get().definition();
    const auto &before=definition.base.get().data(),&after=next.base.get().data();
    if(addedMeshes.size()>32||removedMeshes.size()>32||visibilityMeshes.size()>32)throw std::length_error("Template surface change limit");
    std::set<std::size_t> added,removed,visibility;
    for(const auto mesh:addedMeshes)if(mesh>=after.meshes.size()||!added.insert(mesh).second)
        throw std::invalid_argument("Invalid or duplicate added template surface");
    for(const auto mesh:removedMeshes)if(mesh>=before.meshes.size()||!removed.insert(mesh).second)
        throw std::invalid_argument("Invalid or duplicate removed template surface");
    for(const auto mesh:visibilityMeshes)if(mesh>=before.meshes.size()||removed.contains(mesh)||!visibility.insert(mesh).second)
        throw std::invalid_argument("Invalid or duplicate template visibility slot");
    if(before.meshes.size()-removed.size()+added.size()!=after.meshes.size()||before.bones.size()!=after.bones.size())
        throw std::invalid_argument("Template change requires retained mesh slots and skeleton");
    for(std::size_t i=0;i<before.bones.size();++i)
        if(before.bones[i].name!=after.bones[i].name||before.bones[i].parent!=after.bones[i].parent)
            throw std::invalid_argument("Template addition skeleton identities changed");
    std::vector<std::optional<std::size_t>> oldToNew(before.meshes.size()),newToOld(after.meshes.size());
    std::size_t old=0;
    for(std::size_t target=0;target<after.meshes.size();++target)if(!added.contains(target))
    {
        while(old<before.meshes.size()&&removed.contains(old))++old;
        if(old>=before.meshes.size())throw std::invalid_argument("Template change omits a retained mesh");
        const auto &a=before.meshes[old],&b=after.meshes[target];oldToNew[old]=target;newToOld[target]=old;
        if(visibility.contains(old))
        {
            if(a.name!=b.name||!a.topology||!b.topology)throw std::invalid_argument("Template visibility requires a retained source mesh");
            visibilityLayout(*a.topology,*b.topology,true);
            if(definition.grounding&&definition.grounding->mesh==old)preserveGroundingSupport(definition,*a.topology,*b.topology);
        }
        else retainedLayout(a,b);
        ++old;
    }
    while(old<before.meshes.size()&&removed.contains(old))++old;
    if(old!=before.meshes.size())throw std::invalid_argument("Template change omits a retained mesh");
    for(const auto source:removed)(void)bindingIndex(definition,source);
    for(const auto target:added)(void)bindingIndex(next,target);
    for(const auto &target:next.targets)if(added.contains(target.mesh))
        throw std::invalid_argument("Added surface has template shape targets");
    const auto rejectAnchor=[&](const TemplateAnchor &anchor)
    {if(added.contains(anchor.mesh))throw std::invalid_argument("Added surface is a joint anchor");};
    for(const auto &joint:next.joints)
    {rejectAnchor(joint.head);rejectAnchor(joint.tail);if(joint.rotationPlane)for(const auto &anchor:*joint.rotationPlane)rejectAnchor(anchor);}
    if(next.grounding&&added.contains(next.grounding->mesh))throw std::invalid_argument("Added surface is a grounding source");
    const auto remap=[&](std::size_t index)->std::size_t
    {
        if(index>=oldToNew.size()||!oldToNew[index])throw std::invalid_argument("Removed surface is required by another template feature");
        return *oldToNew[index];
    };
    for(auto &target:definition.targets)target.mesh=remap(target.mesh);
    const auto anchor=[&](TemplateAnchor &value){value.mesh=remap(value.mesh);};
    for(auto &joint:definition.joints)
    {anchor(joint.head);anchor(joint.tail);if(joint.rotationPlane)for(auto &value:*joint.rotationPlane)anchor(value);}
    if(definition.grounding)definition.grounding->mesh=remap(definition.grounding->mesh);
    std::vector<SurfaceBinding> retainedBindings;std::vector<std::optional<SurfaceBindingBasis>> retainedBases;
    for(std::size_t i=0;i<definition.surfaces.size();++i)
    {
        auto binding=definition.surfaces[i];if(removed.contains(binding.targetMesh))continue;
        const auto source=binding.sourceMesh;
        if(visibility.contains(source))remapVisibilityBinding(binding,*before.meshes[source].topology,*after.meshes[remap(source)].topology);
        binding.sourceMesh=remap(source);binding.targetMesh=remap(binding.targetMesh);retainedBindings.push_back(std::move(binding));
        if(!definition.surfaceOffsetBases.empty())retainedBases.push_back(definition.surfaceOffsetBases[i]);
    }
    definition.surfaces=std::move(retainedBindings);definition.surfaceOffsetBases=std::move(retainedBases);
    for(auto &rule:definition.fittings)
    {rule.targetMesh=remap(rule.targetMesh);for(auto &obstacle:rule.obstacleMeshes)obstacle=remap(obstacle);}
    for(const auto &rule:next.fittings)
    {
        if(added.contains(rule.targetMesh))definition.fittings.push_back(rule);
        else if(std::any_of(rule.obstacleMeshes.begin(),rule.obstacleMeshes.end(),[&](auto index){return added.contains(index);}))
            throw std::invalid_argument("Added surface participates in a retained fitting rule");
    }
    std::vector<std::shared_ptr<object3d_detail::Adjustment>> adjustments(state->layers.size());
    for(std::size_t i=0;i<state->layers.size();++i)
    {
        auto &layer=state->layers[i];const auto &previous=*layer.adjustment;
        for(const auto &row:previous.meshes)if(removed.contains(row.mesh)&&!options.discardRemovedEdits)
            throw std::invalid_argument("Removed surface has edit layers");
        const bool changed=previous.materials||previous.images||std::any_of(previous.meshes.begin(),previous.meshes.end(),
            [&](const auto &row){return removed.contains(row.mesh)||remap(row.mesh)!=row.mesh;});
        if(changed)
        {
            auto copy=std::make_shared<object3d_detail::Adjustment>(previous);
            std::erase_if(copy->meshes,[&](const auto &row){return removed.contains(row.mesh);});
            for(auto &row:copy->meshes)row.mesh=remap(row.mesh);adjustments[i]=copy;layer.adjustment=std::move(copy);
        }
        if(layer.pointMasks)
        {
            auto rows=*layer.pointMasks;bool changedMasks=false;
            for(const auto &row:rows)if(removed.contains(row.mesh)&&!options.discardRemovedEdits)
                throw std::invalid_argument("Removed surface has layer masks");
            const auto previousSize=rows.size();std::erase_if(rows,[&](const auto &row){return removed.contains(row.mesh);});
            changedMasks=rows.size()!=previousSize;
            for(auto &row:rows){const auto mapped=remap(row.mesh);changedMasks|=mapped!=row.mesh;row.mesh=mapped;}
            if(changedMasks)layer.pointMasks=rows.empty()?nullptr:std::make_shared<const std::vector<MeshPointMask>>(std::move(rows));
        }
    }
    const auto oldSelections=history_[cursor_].selections;
    std::shared_ptr<const object3d_detail::PointSelectionState> selections=oldSelections;bool selectionChanged=false;
    if(oldSelections)
    {
        auto copy=std::make_shared<object3d_detail::PointSelectionState>(*oldSelections);
        for(auto &entry:copy->entries)if(entry.masks)
        {
            auto rows=*entry.masks;bool changed=false;
            for(const auto &row:rows)if(removed.contains(row.mesh)&&!options.discardRemovedEdits)
                throw std::invalid_argument("Removed surface has a named point selection");
            const auto previousSize=rows.size();std::erase_if(rows,[&](const auto &row){return removed.contains(row.mesh);});
            changed=rows.size()!=previousSize;
            for(auto &row:rows){const auto mapped=remap(row.mesh);changed|=mapped!=row.mesh;row.mesh=mapped;}
            if(changed){selectionChanged=true;entry.masks=rows.empty()?nullptr:std::make_shared<const std::vector<MeshPointMask>>(std::move(rows));}
        }
        if(selectionChanged)selections=std::move(copy);
    }
    auto base=before;base.meshes.clear();base.meshes.reserve(after.meshes.size());
    for(std::size_t target=0;target<after.meshes.size();++target)
    {
        if(added.contains(target)){base.meshes.push_back(after.meshes[target]);continue;}
        const auto source=*newToOld[target];auto part=before.meshes[source];
        if(visibility.contains(source))
        {
            const auto &candidate=*after.meshes[target].topology;auto layout=candidate.layout->definition();
            layout.polygonMaterials=part.topology->layout->definition().polygonMaterials;
            const auto triangles=candidate.layout->triangles(),faces=candidate.layout->trianglePolygons();
            part.topology->layout=MeshTopologyLayout::restore(std::move(layout),part.topology->points,
                std::vector<std::uint32_t>(triangles.begin(),triangles.end()),std::vector<std::uint32_t>(faces.begin(),faces.end()));
        }
        base.meshes.push_back(std::move(part));
    }
    AdditionResources resources(base,after,adjustments);
    for(std::size_t target=0;target<after.meshes.size();++target)if(newToOld[target])
    {
        const auto &a=before.meshes[*newToOld[target]],&b=after.meshes[target];resources.linkMaterial(b.material,a.material);
        if(a.topology)
        {
            const auto &x=a.topology->layout->definition().polygonMaterials,&y=b.topology->layout->definition().polygonMaterials;
            const auto count=a.topology->layout->definition().polygons.size();
            if(!x.empty()||!y.empty())for(std::size_t i=0;i<count;++i)resources.linkMaterial(y.empty()?b.material:y.at(i),x.empty()?a.material:x.at(i));
        }
        else if(!a.triangleMaterials.empty()||!b.triangleMaterials.empty())
            for(std::size_t i=0;i<a.geometry.triangles.size()/3;++i)
                resources.linkMaterial(b.triangleMaterials.empty()?b.material:b.triangleMaterials.at(i),a.triangleMaterials.empty()?a.material:a.triangleMaterials.at(i));
    }
    resources.linkRetainedImages();
    for(const auto target:added)
    {
        auto &part=base.meshes[target];const auto original=part.material;part.material=resources.material(original);
        bool changed=original!=part.material;
        for(auto &value:part.triangleMaterials){const auto nextMaterial=resources.material(value);changed|=value!=nextMaterial;value=nextMaterial;}
        if(part.topology)
        {
            auto layout=part.topology->layout->definition();
            for(auto &value:layout.polygonMaterials){const auto mapped=resources.material(value);changed|=value!=mapped;value=static_cast<std::uint32_t>(mapped);}
            if(changed&&(part.topology->sourceTangents||part.topology->sourceGlbTangents))
                throw std::invalid_argument("Added material remapping would invalidate authored tangents");
            if(changed)
            {
                const auto triangles=part.topology->layout->triangles(),faces=part.topology->layout->trianglePolygons();
                part.topology->layout=MeshTopologyLayout::restore(std::move(layout),part.topology->points,
                    std::vector<std::uint32_t>(triangles.begin(),triangles.end()),std::vector<std::uint32_t>(faces.begin(),faces.end()));
            }
        }
    }
    std::set<std::size_t> completed;const auto &current=object().get().data();
    while(completed.size()<added.size())
    {
        bool advanced=false;
        for(const auto target:added)if(!completed.contains(target))
        {
            const auto index=bindingIndex(next,target);const auto &binding=next.surfaces[index];
            if(added.contains(binding.sourceMesh)&&!completed.contains(binding.sourceMesh))continue;
            if(binding.sourceMesh>=after.meshes.size())throw std::invalid_argument("Added surface source is invalid");
            const auto basis=bindingBasis(next,index);
            if(newToOld[binding.sourceMesh])
            {
                const auto &a=before.meshes[*newToOld[binding.sourceMesh]],&b=after.meshes[binding.sourceMesh];
                if(a.topology&&b.topology)
                {
                    if(a.topology->points.size()!=b.topology->points.size())throw std::invalid_argument("Added surface source points changed");
                    for(std::size_t i=0;i<a.topology->points.size();++i)if(a.topology->points[i].position!=b.topology->points[i].position)
                        throw std::invalid_argument("Added surface neutral source changed");
                }
                else if(!a.topology&&!b.topology)
                {
                    if(a.geometry.vertices.size()!=b.geometry.vertices.size())throw std::invalid_argument("Added surface source vertices changed");
                    for(std::size_t i=0;i<a.geometry.vertices.size();++i)if(a.geometry.vertices[i].position!=b.geometry.vertices[i].position)
                        throw std::invalid_argument("Added surface neutral source changed");
                }
                else throw std::invalid_argument("Added surface source representation changed");
            }
            if(basis)fitMeshSurfaceBindingWithBasis(base,binding,*basis);else fitMeshSurfaceBinding(base,binding);
            auto weighted=base;auto &source=weighted.meshes[binding.sourceMesh];
            if(newToOld[binding.sourceMesh])
            {
                const auto &edited=current.meshes[*newToOld[binding.sourceMesh]];
                if(source.topology&&edited.topology)
                {
                    if(source.topology->points.size()!=edited.topology->points.size())throw std::invalid_argument("Added surface source skin identities changed");
                    for(std::size_t i=0;i<source.topology->points.size();++i)source.topology->points[i].weights=edited.topology->points[i].weights;
                    source.topology->skinning=edited.topology->skinning;
                }
                else if(!source.topology&&!edited.topology&&source.geometry.vertices.size()==edited.geometry.vertices.size())
                {
                    for(std::size_t i=0;i<source.geometry.vertices.size();++i)source.geometry.vertices[i].weights=edited.geometry.vertices[i].weights;
                    source.geometry.skinning=edited.geometry.skinning;
                }
                else throw std::invalid_argument("Added surface source skin identities changed");
            }
            if(basis)(void)transferMeshBindingWeightsWithBasis(weighted,binding,*basis,0,NoBone,SkinningProfileTransfer::Source);
            else (void)transferMeshBindingWeights(weighted,binding,0,NoBone,SkinningProfileTransfer::Source);
            base.meshes[target]=std::move(weighted.meshes[target]);
            if(basis&&definition.surfaceOffsetBases.empty())definition.surfaceOffsetBases.resize(definition.surfaces.size());
            definition.surfaces.push_back(binding);
            if(!definition.surfaceOffsetBases.empty())definition.surfaceOffsetBases.push_back(basis);
            completed.insert(target);advanced=true;
        }
        if(!advanced)throw std::invalid_argument("Added surface binding graph is cyclic");
    }
    Object3DHandle newBase;Object3DTemplateHandle program;
    try
    {
        newBase=owner_.createObject(std::move(base));definition.base=newBase;program=owner_.createTemplate(std::move(definition));state->program=program;
        auto result=rebuild(*state);
        const auto label=removed.empty()?"Add template surfaces":"Change template surfaces";
        if(!selectionChanged)publish(std::move(result),label,std::move(state));
        else
        {
            checkRevision();object3d_detail::validateRemappedPointSelectionTransition(object().get().data(),result,*oldSelections,*selections,oldToNew,
                removed.empty()?object3d_detail::SelectionRemapMode::Insertion:object3d_detail::SelectionRemapMode::Mixed);
            std::vector<Entry> history(history_.begin(),history_.begin()+static_cast<std::ptrdiff_t>(cursor_+1));
            history.reserve(history.size()+1);auto updated=owner_.createObject(std::move(result));
            auto mapping=std::make_shared<const std::vector<std::optional<std::size_t>>>(std::move(oldToNew));
            history.push_back({std::move(updated),label,std::move(state),std::move(selections),std::move(mapping)});
            if(history.size()>limit_+1)history.erase(history.begin());
            history_.swap(history);cursor_=history_.size()-1;++revision_;history.clear();owner_.collectGarbage();
        }
    }
    catch(...){if(program.valid())owner_.eraseTemplate(program.id());if(newBase.valid())owner_.eraseObject(newBase.id());throw;}
    return true;
}

bool Object3DEditor::removeTemplateSurfaces(const Object3DTemplateHandle &alternative,
    std::span<const std::size_t> meshes,std::span<const std::size_t> visibilityMeshes,
    const TemplateSurfaceRemovalOptions &options)
{
    if(meshes.empty())return replaceTemplateSurfaces(alternative,{},visibilityMeshes);
    if(!layered())throw std::logic_error("Template surface removal requires a template editor");
    const auto &next=alternative.get().definition();
    if(!owner_.owns(next.base))throw std::invalid_argument("Template surface removal requires the same engine");
    auto state=copyLayers();auto definition=state->program.get().definition();
    const auto &before=definition.base.get().data(),&after=next.base.get().data();
    if(meshes.size()>32||visibilityMeshes.size()>32)throw std::length_error("Template surface removal limit");
    std::set<std::size_t> removed,visibility;
    for(const auto mesh:meshes)if(mesh>=before.meshes.size()||!removed.insert(mesh).second)
        throw std::invalid_argument("Invalid or duplicate removed template surface");
    for(const auto mesh:visibilityMeshes)if(mesh>=before.meshes.size()||removed.contains(mesh)||!visibility.insert(mesh).second)
        throw std::invalid_argument("Invalid or duplicate template visibility slot");
    if(before.meshes.size()!=after.meshes.size()+removed.size()||before.bones.size()!=after.bones.size())
        throw std::invalid_argument("Template removal requires retained mesh slots and skeleton");
    for(std::size_t i=0;i<before.bones.size();++i)
        if(before.bones[i].name!=after.bones[i].name||before.bones[i].parent!=after.bones[i].parent)
            throw std::invalid_argument("Template removal skeleton identities changed");
    std::vector<std::optional<std::size_t>> map(before.meshes.size());
    std::size_t destination=0;
    for(std::size_t old=0;old<before.meshes.size();++old)if(!removed.contains(old))
    {
        map[old]=destination;const auto &a=before.meshes[old],&b=after.meshes[destination++];
        if(visibility.contains(old))
        {
            if(a.name!=b.name||!a.topology||!b.topology)throw std::invalid_argument("Template visibility requires a retained source mesh");
            visibilityLayout(*a.topology,*b.topology,true);
            if(definition.grounding&&definition.grounding->mesh==old)preserveGroundingSupport(definition,*a.topology,*b.topology);
        }
        else retainedLayout(a,b);
    }
    for(const auto mesh:removed)(void)bindingIndex(definition,mesh);
    const auto remap=[&](std::size_t old)->std::size_t
    {
        if(old>=map.size()||!map[old])throw std::invalid_argument("Removed surface is required by another template feature");
        return *map[old];
    };
    for(auto &target:definition.targets)target.mesh=remap(target.mesh);
    const auto anchor=[&](TemplateAnchor &value){value.mesh=remap(value.mesh);};
    for(auto &joint:definition.joints)
    {anchor(joint.head);anchor(joint.tail);if(joint.rotationPlane)for(auto &value:*joint.rotationPlane)anchor(value);}
    if(definition.grounding)definition.grounding->mesh=remap(definition.grounding->mesh);
    std::vector<SurfaceBinding> bindings;std::vector<std::optional<SurfaceBindingBasis>> bases;
    for(std::size_t i=0;i<definition.surfaces.size();++i)
    {
        auto binding=definition.surfaces[i];if(removed.contains(binding.targetMesh))continue;
        const auto source=binding.sourceMesh;
        if(visibility.contains(source))remapVisibilityBinding(binding,*before.meshes[source].topology,*after.meshes[remap(source)].topology);
        binding.sourceMesh=remap(source);binding.targetMesh=remap(binding.targetMesh);bindings.push_back(std::move(binding));
        if(!definition.surfaceOffsetBases.empty())bases.push_back(definition.surfaceOffsetBases[i]);
    }
    definition.surfaces=std::move(bindings);definition.surfaceOffsetBases=std::move(bases);
    for(auto &rule:definition.fittings)
    {rule.targetMesh=remap(rule.targetMesh);for(auto &obstacle:rule.obstacleMeshes)obstacle=remap(obstacle);}
    for(auto &layer:state->layers)
    {
        const auto &adjustment=*layer.adjustment;bool changed=false;
        for(const auto &entry:adjustment.meshes)
        {
            if(removed.contains(entry.mesh)&&!options.discardRemovedEdits)throw std::invalid_argument("Removed surface has edit layers");
            if(!map.at(entry.mesh)||*map[entry.mesh]!=entry.mesh)changed=true;
        }
        if(changed)
        {
            auto copy=std::make_shared<object3d_detail::Adjustment>(adjustment);
            std::erase_if(copy->meshes,[&](const auto &entry){return removed.contains(entry.mesh);});
            for(auto &entry:copy->meshes)entry.mesh=remap(entry.mesh);layer.adjustment=std::move(copy);
        }
        if(layer.pointMasks)
        {
            auto copy=*layer.pointMasks;bool masksChanged=false;
            for(const auto &row:copy)
            {
                if(removed.contains(row.mesh)&&!options.discardRemovedEdits)throw std::invalid_argument("Removed surface has layer masks");
                if(!map.at(row.mesh)||*map[row.mesh]!=row.mesh)masksChanged=true;
            }
            if(masksChanged)
            {
                std::erase_if(copy,[&](const auto &row){return removed.contains(row.mesh);});
                for(auto &row:copy)row.mesh=remap(row.mesh);
                layer.pointMasks=copy.empty()?nullptr:std::make_shared<const std::vector<MeshPointMask>>(std::move(copy));
            }
        }
    }
    const auto oldSelections=history_[cursor_].selections;
    std::shared_ptr<const object3d_detail::PointSelectionState> selections=oldSelections;
    bool selectionChanged=false;
    if(oldSelections)
    {
        auto copy=std::make_shared<object3d_detail::PointSelectionState>(*oldSelections);
        for(auto &entry:copy->entries)if(entry.masks)
        {
            auto rows=*entry.masks;bool changed=false;
            for(const auto &row:rows)
            {
                if(removed.contains(row.mesh)&&!options.discardRemovedEdits)throw std::invalid_argument("Removed surface has a named point selection");
                if(!map.at(row.mesh)||*map[row.mesh]!=row.mesh)changed=true;
            }
            if(changed)
            {
                selectionChanged=true;std::erase_if(rows,[&](const auto &row){return removed.contains(row.mesh);});
                for(auto &row:rows)row.mesh=remap(row.mesh);
                entry.masks=rows.empty()?nullptr:std::make_shared<const std::vector<MeshPointMask>>(std::move(rows));
            }
        }
        if(selectionChanged)selections=std::move(copy);
    }
    auto base=before;std::vector<Object3DMesh> retained;retained.reserve(after.meshes.size());
    for(std::size_t old=0;old<before.meshes.size();++old)if(map[old])
    {
        auto part=before.meshes[old];
        if(visibility.contains(old))
        {
            const auto &candidate=*after.meshes[*map[old]].topology;auto layout=candidate.layout->definition();
            layout.polygonMaterials=part.topology->layout->definition().polygonMaterials;
            const auto triangles=candidate.layout->triangles(),faces=candidate.layout->trianglePolygons();
            part.topology->layout=MeshTopologyLayout::restore(std::move(layout),part.topology->points,
                std::vector<std::uint32_t>(triangles.begin(),triangles.end()),std::vector<std::uint32_t>(faces.begin(),faces.end()));
        }
        retained.push_back(std::move(part));
    }
    base.meshes=std::move(retained);Object3DHandle newBase;Object3DTemplateHandle program;
    try
    {
        newBase=owner_.createObject(std::move(base));definition.base=newBase;program=owner_.createTemplate(std::move(definition));state->program=program;
        auto result=rebuild(*state);
        if(!selectionChanged)publish(std::move(result),"Remove template surfaces",std::move(state));
        else
        {
            checkRevision();object3d_detail::validateRemappedPointSelectionTransition(object().get().data(),result,*oldSelections,*selections,map);
            std::vector<Entry> history(history_.begin(),history_.begin()+static_cast<std::ptrdiff_t>(cursor_+1));
            history.reserve(history.size()+1);auto updated=owner_.createObject(std::move(result));
            auto stored=std::make_shared<const std::vector<std::optional<std::size_t>>>(std::move(map));
            history.push_back({std::move(updated),"Remove template surfaces",std::move(state),std::move(selections),std::move(stored)});
            if(history.size()>limit_+1)history.erase(history.begin());
            history_.swap(history);cursor_=history_.size()-1;++revision_;history.clear();owner_.collectGarbage();
        }
    }
    catch(...){if(program.valid())owner_.eraseTemplate(program.id());if(newBase.valid())owner_.eraseObject(newBase.id());throw;}
    return true;
}

bool Object3DEditor::replaceTemplateSurfaces(const Object3DTemplateHandle &alternative,
    std::span<const std::size_t> meshes,std::span<const std::size_t> visibilityMeshes)
{
    if(!layered())throw std::logic_error("Template surface replacement requires a template editor");
    const auto &next=alternative.get().definition();
    if(!owner_.owns(next.base))throw std::invalid_argument("Template surface replacement requires the same engine");
    auto state=copyLayers();auto definition=state->program.get().definition();
    const auto &before=definition.base.get().data(),&after=next.base.get().data();
    if(before.meshes.size()!=after.meshes.size()||before.bones.size()!=after.bones.size())
        throw std::invalid_argument("Template surface replacement requires stable mesh slots and skeleton");
    for(std::size_t i=0;i<before.bones.size();++i)
        if(before.bones[i].name!=after.bones[i].name||before.bones[i].parent!=after.bones[i].parent)
            throw std::invalid_argument("Template surface replacement skeleton identities changed");
    if(meshes.size()>32||visibilityMeshes.size()>32)throw std::length_error("Template surface replacement limit");
    std::set<std::size_t> replaced,visibility;
    for(const auto mesh:meshes)if(mesh>=before.meshes.size()||!replaced.insert(mesh).second)
        throw std::invalid_argument("Invalid or duplicate template surface slot");
    for(const auto mesh:visibilityMeshes)if(mesh>=before.meshes.size()||replaced.contains(mesh)||!visibility.insert(mesh).second)
        throw std::invalid_argument("Invalid or duplicate template visibility slot");
    bool visibilityChanged=false;
    for(std::size_t i=0;i<before.meshes.size();++i)
    {
        if(replaced.contains(i))continue;
        const auto &old=before.meshes[i],&candidate=after.meshes[i];
        if(old.name!=candidate.name||old.topology.has_value()!=candidate.topology.has_value())
            throw std::invalid_argument("Retained template mesh identity changed");
        if(old.topology)
        {
            if(visibility.contains(i))
            {
                visibilityLayout(*old.topology,*candidate.topology,true);
                if(definition.grounding&&definition.grounding->mesh==i)
                    preserveGroundingSupport(definition,*old.topology,*candidate.topology);
                visibilityChanged|=old.topology->layout->definition().hiddenPolygons!=candidate.topology->layout->definition().hiddenPolygons;
            }
            else retainedLayout(old,candidate);
        }
        else if(visibility.contains(i)||old.geometry.triangles!=candidate.geometry.triangles||old.geometry.vertices.size()!=candidate.geometry.vertices.size())
            throw std::invalid_argument("Retained template render topology changed");
    }
    if(replaced.empty()&&!visibilityChanged)return false;
    for(const auto &target:definition.targets)if(replaced.contains(target.mesh))throw std::invalid_argument("Replacement surface is a template target source");
    const auto checkAnchor=[&](const TemplateAnchor &anchor){if(replaced.contains(anchor.mesh))throw std::invalid_argument("Replacement surface is a joint anchor");};
    for(const auto &joint:definition.joints)
    {checkAnchor(joint.head);checkAnchor(joint.tail);if(joint.rotationPlane)for(const auto &anchor:*joint.rotationPlane)checkAnchor(anchor);}
    if(definition.grounding&&replaced.contains(definition.grounding->mesh))throw std::invalid_argument("Replacement surface is a grounding source");
    for(const auto &binding:definition.surfaces)if(replaced.contains(binding.sourceMesh))throw std::invalid_argument("Replacement surface has dependent followers");
    for(auto &binding:definition.surfaces)
        if(!replaced.contains(binding.targetMesh)&&visibility.contains(binding.sourceMesh))
            remapVisibilityBinding(binding,*before.meshes[binding.sourceMesh].topology,*after.meshes[binding.sourceMesh].topology);
    for(const auto &rule:definition.fittings)
        if(replaced.contains(rule.targetMesh)||std::any_of(rule.obstacleMeshes.begin(),rule.obstacleMeshes.end(),[&](auto mesh){return replaced.contains(mesh);}))
            throw std::invalid_argument("Replacement surface participates in automatic fitting");
    if(const auto &selection=history_[cursor_].selections)
        for(const auto &entry:selection->entries)if(entry.masks)for(const auto &mask:*entry.masks)
            if(replaced.contains(mask.mesh))throw std::invalid_argument("Replacement surface has a named point selection");
    for(const auto &layer:state->layers)
    {
        if(std::any_of(layer.adjustment->meshes.begin(),layer.adjustment->meshes.end(),[&](const auto &mesh){return replaced.contains(mesh.mesh);}))
            throw std::invalid_argument("Replacement surface has edit layers");
        if(layer.pointMasks&&std::any_of(layer.pointMasks->begin(),layer.pointMasks->end(),[&](const auto &mask){return replaced.contains(mask.mesh);}))
            throw std::invalid_argument("Replacement surface has layer masks");
    }
    struct MaterialSwap final {std::size_t index;Object3DMaterial replacement;};
    struct ImageSwap final {std::size_t index;Object3DImage replacement;};
    struct BindingSwap final {std::size_t index;SurfaceBinding binding;std::optional<SurfaceBindingBasis> basis;};
    std::vector<MaterialSwap> materials;std::vector<ImageSwap> images;std::vector<BindingSwap> bindings;
    std::set<std::size_t> oldMaterials,newMaterials,oldImages,newImages;
    for(const auto mesh:replaced)
    {
        const auto &old=before.meshes[mesh],&candidate=after.meshes[mesh];singleMaterial(old);singleMaterial(candidate);
        if(!oldMaterials.insert(old.material).second||!newMaterials.insert(candidate.material).second)
            throw std::invalid_argument("Replacement surfaces share a material");
        for(std::size_t i=0;i<before.meshes.size();++i)if(i!=mesh&&usesMaterial(before.meshes[i],old.material))
            throw std::invalid_argument("Replacement material is shared");
        for(std::size_t i=0;i<after.meshes.size();++i)if(i!=mesh&&usesMaterial(after.meshes[i],candidate.material))
            throw std::invalid_argument("Replacement candidate material is shared");
        const auto &oldMaterial=before.materials.at(old.material);auto replacement=after.materials.at(candidate.material);
        const auto mapImage=[&](std::optional<std::size_t> previous,std::optional<std::size_t> incoming)->std::optional<std::size_t>
        {
            if(previous.has_value()!=incoming.has_value())throw std::invalid_argument("Replacement image slot count changed");
            if(!previous)return std::nullopt;
            if(!oldImages.insert(*previous).second||!newImages.insert(*incoming).second)throw std::invalid_argument("Replacement image is shared");
            for(std::size_t i=0;i<before.materials.size();++i)if(i!=old.material&&usesImage(before.materials[i],*previous))
                throw std::invalid_argument("Replacement image is shared by another material");
            for(std::size_t i=0;i<after.materials.size();++i)if(i!=candidate.material&&usesImage(after.materials[i],*incoming))
                throw std::invalid_argument("Replacement candidate image is shared by another material");
            images.push_back({*previous,after.images.at(*incoming)});return previous;
        };
        replacement.baseColorImage=mapImage(oldMaterial.baseColorImage,replacement.baseColorImage);
        replacement.normalImage=mapImage(oldMaterial.normalImage,replacement.normalImage);
        materials.push_back({old.material,std::move(replacement)});
        const auto previousBinding=bindingIndex(definition,mesh),nextBinding=bindingIndex(next,mesh);
        const auto &incoming=next.surfaces[nextBinding];
        if(incoming.sourceMesh!=definition.surfaces[previousBinding].sourceMesh||replaced.contains(incoming.sourceMesh))
            throw std::invalid_argument("Replacement follower source changed");
        bindings.push_back({previousBinding,incoming,bindingBasis(next,nextBinding)});
    }
    for(auto &layer:state->layers)
    {
        const auto &existing=*layer.adjustment;
        for(const auto &mesh:existing.meshes)
        {
            if(mesh.material&&oldMaterials.contains(*mesh.material))
                throw std::invalid_argument("Replacement material is assigned by an edit layer");
            if(mesh.triangleMaterials&&std::any_of(mesh.triangleMaterials->begin(),mesh.triangleMaterials->end(),
                [&](auto material){return oldMaterials.contains(material);}))
                throw std::invalid_argument("Replacement material is assigned by an edit layer");
        }
        if((materials.empty()||!existing.materials)&&(images.empty()||!existing.images))continue;
        auto adjustment=std::make_shared<object3d_detail::Adjustment>(existing);
        if(adjustment->materials)
        {
            for(const auto &item:materials)
            {
                if(item.index>=adjustment->materials->size()||!sameMaterial(adjustment->materials->at(item.index),before.materials.at(item.index)))
                    throw std::invalid_argument("Replacement material has an edit layer");
                adjustment->materials->at(item.index)=item.replacement;
            }
            for(std::size_t i=0;i<adjustment->materials->size();++i)if(!oldMaterials.contains(i))
                for(const auto &image:images)if(usesImage(adjustment->materials->at(i),image.index))
                    throw std::invalid_argument("Replacement image is shared by an edit layer");
        }
        if(adjustment->images)for(const auto &item:images)
        {
            if(item.index>=adjustment->images->size()||adjustment->images->at(item.index)!=before.images.at(item.index))
                throw std::invalid_argument("Replacement image has an edit layer");
            adjustment->images->at(item.index)=item.replacement;
        }
        layer.adjustment=std::move(adjustment);
    }
    auto base=before;
    for(const auto mesh:visibility)
    {
        auto &part=base.meshes[mesh];const auto &candidate=*after.meshes[mesh].topology;
        auto layout=candidate.layout->definition();layout.polygonMaterials=part.topology->layout->definition().polygonMaterials;
        const auto triangles=candidate.layout->triangles(),faces=candidate.layout->trianglePolygons();
        part.topology->layout=MeshTopologyLayout::restore(std::move(layout),part.topology->points,
            std::vector<std::uint32_t>(triangles.begin(),triangles.end()),std::vector<std::uint32_t>(faces.begin(),faces.end()));
    }
    for(const auto &item:materials)base.materials[item.index]=item.replacement;
    for(const auto &item:images)base.images[item.index]=item.replacement;
    for(const auto mesh:replaced)
    {
        auto incoming=after.meshes[mesh];const auto oldMaterial=before.meshes[mesh].material;
        if(incoming.topology&&incoming.material!=oldMaterial)
        {
            if(incoming.topology->sourceTangents||incoming.topology->sourceGlbTangents)
                throw std::invalid_argument("Replacement material remapping would invalidate authored tangents");
            auto layout=incoming.topology->layout->definition();
            for(auto &index:layout.polygonMaterials)index=static_cast<std::uint32_t>(oldMaterial);
            const auto triangles=incoming.topology->layout->triangles(),faces=incoming.topology->layout->trianglePolygons();
            incoming.topology->layout=MeshTopologyLayout::restore(std::move(layout),incoming.topology->points,
                std::vector<std::uint32_t>(triangles.begin(),triangles.end()),std::vector<std::uint32_t>(faces.begin(),faces.end()));
        }
        for(auto &material:incoming.triangleMaterials)material=oldMaterial;incoming.material=oldMaterial;base.meshes[mesh]=std::move(incoming);
    }
    for(const auto &item:bindings)
    {
        definition.surfaces[item.index]=item.binding;
        if(item.basis&&definition.surfaceOffsetBases.empty())definition.surfaceOffsetBases.resize(definition.surfaces.size());
        if(!definition.surfaceOffsetBases.empty())definition.surfaceOffsetBases[item.index]=item.basis;
        if(item.basis)fitMeshSurfaceBindingWithBasis(base,item.binding,*item.basis);
        else fitMeshSurfaceBinding(base,item.binding);
        // Bind against the user's current source skin, while keeping the new
        // follower's positions in the template's neutral coordinate state.
        auto weighted=base;auto &source=weighted.meshes.at(item.binding.sourceMesh);
        const auto &current=object().get().data().meshes.at(item.binding.sourceMesh);
        if(source.topology&&current.topology)
        {
            if(source.topology->points.size()!=current.topology->points.size())throw std::invalid_argument("Replacement source point identities changed");
            for(std::size_t i=0;i<source.topology->points.size();++i)source.topology->points[i].weights=current.topology->points[i].weights;
            source.topology->skinning=current.topology->skinning;
        }
        else if(!source.topology&&!current.topology&&source.geometry.vertices.size()==current.geometry.vertices.size())
        {
            for(std::size_t i=0;i<source.geometry.vertices.size();++i)source.geometry.vertices[i].weights=current.geometry.vertices[i].weights;
            source.geometry.skinning=current.geometry.skinning;
        }
        else throw std::invalid_argument("Replacement source skin identities changed");
        if(item.basis)(void)transferMeshBindingWeightsWithBasis(weighted,item.binding,*item.basis,0,NoBone,SkinningProfileTransfer::Source);
        else (void)transferMeshBindingWeights(weighted,item.binding,0,NoBone,SkinningProfileTransfer::Source);
        base.meshes[item.binding.targetMesh]=std::move(weighted.meshes[item.binding.targetMesh]);
    }
    Object3DHandle newBase;Object3DTemplateHandle newProgram;
    try
    {
        newBase=owner_.createObject(std::move(base));definition.base=newBase;newProgram=owner_.createTemplate(std::move(definition));state->program=newProgram;
        auto result=rebuild(*state);publish(std::move(result),"Replace template surfaces",std::move(state));
    }
    catch(...){if(newProgram.valid())owner_.eraseTemplate(newProgram.id());if(newBase.valid())owner_.eraseObject(newBase.id());throw;}
    return true;
}
} // namespace iiActorObject
