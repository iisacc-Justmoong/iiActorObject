#include "Object3DAsset.h"
#include "CharacterMath.h"
#include "MeshGeometryCore.h"
#include "Object3DNames.h"
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace iiActorObject
{
using namespace character_detail;
using object3d_detail::validateName;
Object3DAsset::Object3DAsset(Object3DData data)
    : data_(std::move(data)), hasRig_(data_.kind == Object3DKind::SkeletalMesh ||
                                      (data_.kind == Object3DKind::Automatic && !data_.bones.empty()))
{
    if (data_.kind != Object3DKind::Automatic && data_.kind != Object3DKind::StaticMesh &&
        data_.kind != Object3DKind::SkeletalMesh)
        throw std::invalid_argument("Unknown 3D object kind");
    if (!hasRig_)
    {
        if (data_.bones.empty())
            data_.bones.push_back({"root", NoBone, {}});
        if (data_.bones.size() != 1 || data_.bones[0].parent != NoBone)
            throw std::invalid_argument("A static 3D mesh needs one rigid root");
        for (auto &mesh : data_.meshes)
            for (auto &vertex : mesh.geometry.vertices)
                if (vertex.weights.empty())
                    vertex.weights.push_back({0, 1});
    }
    data_.kind = hasRig_ ? Object3DKind::SkeletalMesh : Object3DKind::StaticMesh;
    validateName(data_.name);
    for (const auto &bone : data_.bones)
        validateName(bone.name);
    skeleton_ = std::make_shared<Skeleton>(data_.bones);
    data_.bones.assign(skeleton_->bones().begin(), skeleton_->bones().end());
    std::unordered_set<std::string> names;
    for (const auto &semantic : data_.semantics)
    {
        validateName(semantic.name);
        if (semantic.bone >= data_.bones.size() || !names.insert(semantic.name).second)
            throw std::invalid_argument("Invalid or duplicate joint semantic");
    }
    if (data_.materials.empty() || data_.meshes.empty())
        throw std::invalid_argument("Character needs materials and meshes");
    if(data_.images.size()>4096)throw std::length_error("Object image count budget");
    std::size_t imageBytes=0;names.clear();
    for(const auto &image:data_.images)
    {
        if(image.empty())throw std::invalid_argument("Empty object image");validateName(image.name());
        if(!names.insert(std::string(image.name())).second)throw std::invalid_argument("Duplicate object image name");
        if(image.encoded().size()>256*1024*1024-imageBytes)throw std::length_error("Object image byte budget");imageBytes+=image.encoded().size();
    }
    names.clear();
    for (const auto &m : data_.materials)
    {
        validateName(m.name);
        if (!names.insert(m.name).second)
            throw std::invalid_argument("Duplicate material");
        for (double c : m.baseColor)
            weight(c);
        weight(m.metallic);
        weight(m.roughness);
        if(m.baseColorImage&&*m.baseColorImage>=data_.images.size())throw std::out_of_range("Material image reference");
        if(m.normalImage&&*m.normalImage>=data_.images.size())throw std::out_of_range("Normal image reference");
        if(!std::isfinite(m.normalScale)||std::abs(m.normalScale)>std::numeric_limits<float>::max())
            throw std::invalid_argument("Invalid normal texture scale");
        if(m.surface.alphaMode!=Object3DAlphaMode::Automatic&&m.surface.alphaMode!=Object3DAlphaMode::Opaque&&
           m.surface.alphaMode!=Object3DAlphaMode::Mask&&m.surface.alphaMode!=Object3DAlphaMode::Blend)
            throw std::invalid_argument("Invalid material alpha mode");
        if(!std::isfinite(m.surface.alphaCutoff)||m.surface.alphaCutoff<0||m.surface.alphaCutoff>std::numeric_limits<float>::max())
            throw std::invalid_argument("Invalid material alpha cutoff");
        if(!m.baseColorUvSet.empty())
        {
            validateName(m.baseColorUvSet);
            if(!m.baseColorImage)throw std::invalid_argument("UV set selection requires a base-color image");
        }
        if(!m.normalUvSet.empty())
        {
            validateName(m.normalUvSet);
            if(!m.normalImage)throw std::invalid_argument("UV set selection requires a normal image");
        }
    }
    names.clear();
    for (auto &part : data_.meshes)
    {
        if (part.topology)
        {
            normalizeMeshTopology(*part.topology,data_.bones.size());
            auto rendered=renderMeshTopology(*part.topology);
            part.geometry=std::move(rendered.geometry);
            part.textureCoordinates=std::move(rendered.textureCoordinates);
            part.primaryUvName=std::move(rendered.primaryUvName);
            part.additionalUvSets=std::move(rendered.additionalUvSets);
            part.triangleMaterials=std::move(rendered.triangleMaterials);
            for(const auto material:part.topology->layout->definition().polygonMaterials)
                if(material>=data_.materials.size())throw std::out_of_range("Source polygon material");
        }
        validateName(part.name);
        if (!names.insert(part.name).second || part.material >= data_.materials.size())
            throw std::invalid_argument("Invalid character mesh name or material");
        for (const auto &target : part.geometry.morphTargets)
            validateName(target.name);
        std::unordered_set<std::string> morphNames, defaultNames;
        for(const auto &target:part.geometry.morphTargets)morphNames.insert(target.name);
        for(const auto &entry:part.morphDefaults)
        {
            if(!std::isfinite(entry.value)||!morphNames.contains(entry.name)||!defaultNames.insert(entry.name).second)
                throw std::invalid_argument("Invalid or duplicate default morph weight");
        }
        std::erase_if(part.morphDefaults,[](const auto &entry){return entry.value==0;});
        if(!part.triangleMaterials.empty() && part.triangleMaterials.size()!=part.geometry.triangles.size()/3)
            throw std::invalid_argument("Triangle material count");
        for(const auto material:part.triangleMaterials)
            if(material>=data_.materials.size())throw std::out_of_range("Triangle material");
        if (!part.textureCoordinates.empty() &&
            part.textureCoordinates.size() != part.geometry.vertices.size())
            throw std::invalid_argument("UV count must match vertices");
        for (const auto uv : part.textureCoordinates)
            if (!std::isfinite(uv.u) || !std::isfinite(uv.v))
                throw std::invalid_argument("Nonfinite UV");
        if(part.additionalUvSets.size()>31)throw std::length_error("UV set limit");
        if(!part.primaryUvName.empty())validateName(part.primaryUvName);
        if(part.textureCoordinates.empty()&&!part.additionalUvSets.empty())
            throw std::invalid_argument("Additional UV sets require a primary set");
        std::unordered_set<std::string> uvNames;
        if(!part.textureCoordinates.empty())uvNames.insert(part.primaryUvName.empty()?"UVMap":part.primaryUvName);
        for(const auto &set:part.additionalUvSets)
        {
            validateName(set.name);
            if(!uvNames.insert(set.name).second||set.coordinates.size()!=part.geometry.vertices.size())
                throw std::invalid_argument("Invalid UV set name or vertex count");
            for(const auto uv:set.coordinates)
                if(!std::isfinite(uv.u)||!std::isfinite(uv.v))throw std::invalid_argument("Nonfinite UV set value");
        }
        const auto checkMaterialUv=[&](std::size_t material)
        {
            const auto &m=data_.materials.at(material);
            if(m.normalImage&&part.textureCoordinates.empty())throw std::invalid_argument("Normal texture requires mesh UV coordinates");
            const auto check=[&](const std::string &name)
            {
                if(name.empty())return;
                const bool primary=!part.textureCoordinates.empty() && name==(part.primaryUvName.empty()?"UVMap":part.primaryUvName);
                const bool additional=std::any_of(part.additionalUvSets.begin(),part.additionalUvSets.end(),
                    [&](const auto &set){return set.name==name;});
                if(!primary&&!additional)throw std::invalid_argument("Material references a UV set absent from its mesh");
            };
            check(m.baseColorUvSet);check(m.normalUvSet);
        };
        if(part.triangleMaterials.empty())checkMaterialUv(part.material);
        else for(const auto material:part.triangleMaterials)checkMaterialUv(material);
        validateMeshGeometry(part.geometry);
        auto runtime =
            std::make_shared<SkeletalMesh>(skeleton_, std::vector<SkeletalMeshLod>{part.geometry});
        // Publish exactly the same normalized weights/normals used by the runtime.
        part.geometry = runtime->lods()[0];
        meshes_.push_back(std::move(runtime));
    }
    validateRigSampleCatalogs(data_.rigSampleCatalogs);
    if(data_.referencePose)
    {
        Pose neutral(skeleton_);referenceLocals_=evaluatePoseReference(neutral,*data_.referencePose);
    }
}
bool Object3DAsset::hasRig() const noexcept
{
    return hasRig_;
}
const Object3DData &Object3DAsset::data() const noexcept
{
    return data_;
}
const std::shared_ptr<const Skeleton> &Object3DAsset::skeleton() const noexcept
{
    return skeleton_;
}
std::span<const std::shared_ptr<const SkeletalMesh>> Object3DAsset::meshes() const noexcept
{
    return meshes_;
}
BoneIndex Object3DAsset::joint(std::string_view semantic) const noexcept
{
    for (const auto &s : data_.semantics)
        if (s.name == semantic)
            return s.bone;
    return NoBone;
}
std::span<const Transform> Object3DAsset::referencePoseLocals() const noexcept
{
    return referenceLocals_;
}
std::unique_ptr<iiCharacter> Object3DAsset::instantiate(std::string instanceName) const
{
    auto instance = std::make_unique<iiCharacter>(
        skeleton_, instanceName.empty() ? data_.name : std::move(instanceName));
    for(std::size_t i=0;i<referenceLocals_.size();++i)instance->pose().setLocal(i,referenceLocals_[i]);
    for (std::size_t i = 0; i < meshes_.size(); ++i)
    {
        auto &component=instance->addMesh(meshes_[i], data_.meshes[i].name);
        std::unordered_map<std::string_view,double> defaults;
        for(const auto &entry:data_.meshes[i].morphDefaults)defaults.emplace(entry.name,entry.value);
        const auto &targets=data_.meshes[i].geometry.morphTargets;
        for(std::size_t target=0;target<targets.size();++target)
            if(const auto found=defaults.find(targets[target].name);found!=defaults.end())
                component.setMorphWeight(target,found->second);
    }
    return instance;
}
} // namespace iiActorObject
