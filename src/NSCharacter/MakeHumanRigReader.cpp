#include "MakeHumanRigReader.h"
#include <array>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <variant>

namespace iiActorObject::NSCharacter::human_detail
{
void loadMakeHumanRig(Object3DData &data,Object3DTemplateDefinition &definition,Files &files)
{
    auto &topology=*data.meshes[0].topology;
    const auto rig=files.json("default.mhskel"),weights=files.json("default_weights.mhw");
    if(as<std::string>(field(rig,"license"))!="CC0" || as<std::string>(field(weights,"license"))!="CC0")
        throw std::invalid_argument("Expected CC0 human rig data");
    const auto &bones=as<Value::Object>(field(rig,"bones"));
    const auto &joints=as<Value::Object>(field(rig,"joints"));
    const auto &planes=as<Value::Object>(field(rig,"planes"));
    if(bones.size()>4096)throw std::length_error("Human rig bone limit");
    std::map<std::string,BoneIndex,std::less<>> indices;std::set<std::string> visiting;
    std::function<BoneIndex(const std::string &)> addBone=[&](const auto &name)->BoneIndex
    {
        if(const auto it=indices.find(name);it!=indices.end())return it->second;
        if(!visiting.insert(name).second)throw std::invalid_argument("Cyclic human rig");
        const auto &b=bones.at(name);const auto &p=field(b,"parent");BoneIndex parent=NoBone;
        if(!std::holds_alternative<std::monostate>(p.storage()))parent=addBone(as<std::string>(p));
        const auto index=data.bones.size();data.bones.push_back({name,parent,{}});indices.emplace(name,index);visiting.erase(name);return index;
    };
    for(const auto &[name,b]:bones) { (void)b;addBone(name); }
    auto anchor=[&](const std::string &name)
    {
        TemplateAnchor a;for(const auto &v:as<Value::Array>(joints.at(name)))a.points.push_back(pointId(v));return a;
    };
    for(const auto &[name,b]:bones)
    {
        const auto bone=indices.at(name);TemplateJointBinding binding;
        binding.bone=bone;binding.head=anchor(as<std::string>(field(b,"head")));binding.tail=anchor(as<std::string>(field(b,"tail")));
        const auto &planeName=field(b,"rotation_plane");
        if(const auto p=std::get_if<std::string>(&planeName.storage()))
        {
            const auto &plane=as<Value::Array>(planes.at(*p));if(plane.size()!=3)throw std::invalid_argument("Invalid rig rotation plane");
            binding.rotationPlane=std::array{anchor(as<std::string>(plane[0])),anchor(as<std::string>(plane[1])),anchor(as<std::string>(plane[2]))};
        }
        definition.joints.push_back(std::move(binding));data.semantics.push_back({name,bone});
    }
    for(const auto &[name,list]:as<Value::Object>(field(weights,"weights")))
    {
        const auto bone=indices.at(name);std::set<std::uint32_t> seen;
        for(const auto &row:as<Value::Array>(list))
        {
            const auto &pair=as<Value::Array>(row);if(pair.size()!=2)throw std::invalid_argument("Invalid human skin row");
            const auto p=pointId(pair[0]);const auto w=numeric(pair[1]);
            if(p>=topology.points.size()||w<=0||!seen.insert(p).second)throw std::invalid_argument("Invalid human skin weight");
            topology.points[p].weights.push_back({bone,w});
        }
    }
    // Every visible body point must have an authored binding; helpers may have none.
    const auto &layout=topology.layout->definition();
    for(const auto r:layout.renderToLayout)
        if(topology.points[layout.layouts[r].position].weights.empty())throw std::invalid_argument("Visible human point has no skin binding");
}
} // namespace iiActorObject::NSCharacter::human_detail
