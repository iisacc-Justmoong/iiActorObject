#include "Object3DProjectAdjustmentValidation.h"
#include "Object3DAdjustmentData.h"
#include "CharacterMath.h"
#include <cmath>
#include <set>
#include <stdexcept>

namespace iiActorObject::object3d_detail
{
void validateProjectAdjustmentReferences(const Adjustment &a,const Object3DData &base)
{
    std::set<std::size_t> meshes,joints;
    for(const auto &m:a.meshes)
    {
        if(!meshes.insert(m.mesh).second)throw std::invalid_argument("Duplicate project mesh adjustment");const auto &part=base.meshes.at(m.mesh);
        if(m.skinning)validateSkinningProfile(*m.skinning,part.topology?part.topology->points.size():part.geometry.vertices.size());
        if((m.sourceMorphs&&!part.topology)||(m.renderMorphs&&part.topology))throw std::invalid_argument("Project morph adjustment source mismatch");
        auto unique=[](const auto &values,auto key){std::set<std::size_t> ids;for(const auto &v:values)if(!ids.insert(key(v)).second)throw std::invalid_argument("Duplicate project sparse adjustment");};
        unique(m.positions,[](const auto &v){return v.point;});unique(m.weights,[](const auto &v){return v.point;});unique(m.uv,[](const auto &v){return v.index;});unique(m.normals,[](const auto &v){return v.index;});
        for(const auto &value:m.normals)
        {
            if(value.index>=(part.topology?part.topology->normals.size():part.geometry.vertices.size()))throw std::out_of_range("Project normal adjustment index");
            const auto q=value.rotation.normalized();
            if(std::abs(q.x-value.rotation.x)>1e-9||std::abs(q.y-value.rotation.y)>1e-9||
               std::abs(q.z-value.rotation.z)>1e-9||std::abs(q.w-value.rotation.w)>1e-9)
                throw std::invalid_argument("Non-unit project normal adjustment");
        }
        std::set<std::size_t> uvSets;
        for(const auto &set:m.additionalUv)
        {
            if(!uvSets.insert(set.set).second)throw std::invalid_argument("Duplicate project UV set adjustment");
            const auto &coordinates=part.topology?part.topology->additionalUvSets.at(set.set).coordinates:
                                                    part.additionalUvSets.at(set.set).coordinates;
            unique(set.values,[](const auto &v){return v.index;});
            for(const auto &value:set.values)
                if(value.index>=coordinates.size())throw std::out_of_range("Project UV set adjustment index");
        }
    }
    for(const auto &j:a.joints)
    {
        if(j.bone>=base.bones.size()||!joints.insert(j.bone).second||!character_detail::finite(j.position)||!character_detail::finite(j.scale)||
           (j.parent&&(j.bone==0?*j.parent!=NoBone:(*j.parent>=base.bones.size()||*j.parent==j.bone)))||
           (j.name&&(j.name->empty()||j.name->find('\0')!=std::string::npos)))throw std::invalid_argument("Invalid project joint adjustment");
        (void)j.rotation.normalized();
    }
}
} // namespace iiActorObject::object3d_detail
