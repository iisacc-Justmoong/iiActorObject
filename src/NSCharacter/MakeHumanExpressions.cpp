#include "MakeHumanExpressions.h"
#include <iiActorObject/Object3DBvh.h>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace iiActorObject::NSCharacter::human_detail
{
Object3DPoseProgram expressionAssets(const Object3DData &data,Files &files,double scale)
{
    const auto catalog=files.json("catalog.json");
    if(as<std::string>(field(catalog,"format"))!="iiActorObject.MakeHumanExpressions.1"||as<std::string>(field(catalog,"baseMesh"))!="hm08"||as<std::string>(field(catalog,"sourceUp"))!="Z")throw std::invalid_argument("Unsupported facial pose catalogue");
    const auto metadata=files.json(as<std::string>(field(catalog,"metadata")));if(as<std::string>(field(metadata,"license"))!="CC0")throw std::invalid_argument("Expected CC0 facial pose data");
    BvhImportOptions options;options.scale=scale;options.basis=Quaternion::fromAxisAngle({1,0,0},-std::numbers::pi/2);
    const auto motion=parseObject3DBvh(files.read(as<std::string>(field(catalog,"motion"))),options);const auto &mapping=as<Value::Array>(field(metadata,"framemapping"));
    if(mapping.size()!=motion.frameCount()||mapping.size()<2||as<std::string>(mapping[0])!="Rest"||motion.joints().size()!=data.bones.size())throw std::invalid_argument("Facial pose dimensions disagree");
    const Skeleton skeleton(data.bones);for(const auto &joint:motion.joints())if(skeleton.findBone(joint.name)==NoBone)throw std::invalid_argument("Facial pose source joint is missing");
    std::vector<PoseUnitFrame> frames;for(std::size_t i=1;i<mapping.size();++i)frames.push_back({as<std::string>(mapping[i]),i});return makeObject3DPoseProgram(motion,frames,0,numeric(field(catalog,"rotationNoiseToleranceRadians")));
}
} // namespace iiActorObject::NSCharacter::human_detail
