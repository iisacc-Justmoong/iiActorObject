#include "HumanoidGenerator.h"
#include "CharacterMath.h"
#include <iiActorObject/ImplicitSurface.h>
#include <iiActorObject/MeshPrimitives.h>

namespace iiActorObject::NSCharacter
{
using namespace character_detail;
namespace
{
struct Finger final
{
    Vector3 start, end;
    double radius;
};
auto fingers()
{
    std::array<Finger, 5> f;
    for (unsigned i = 0; i < 5; ++i)
    {
        const double lengths[]{.059, .083, .092, .085, .067};
        f[i] = {{.702, .999, (static_cast<double>(i) - 2) * .017}, {}, .008};
        f[i].end = add(f[i].start, {lengths[i] * .71, -lengths[i] * .71, 0});
    }
    f[0] = {{.665, 1.027, .033}, {.721, .991, .069}, .0095};
    return f;
}
double humanField(Vector3 p, double sampleRadius)
{
    p.x = std::abs(p.x);
    auto d = ellipsoidSignedDistance(p, {0, .955, -.008}, {.161, .185, .108});
    const auto merge = [&](double value) { d = smoothUnionDistance(d, value, .022); };
    merge(ellipsoidSignedDistance(p, {0, 1.15, 0}, {.137, .215, .094}));
    merge(ellipsoidSignedDistance(p, {0, 1.355, 0}, {.194, .185, .112}));
    merge(capsuleSignedDistance(p, {0, 1.45, 0}, {0, 1.59, 0}, .052));
    merge(ellipsoidSignedDistance(p, {0, 1.675, .005}, {.087, .125, .091}));
    merge(ellipsoidSignedDistance(p, {0, 1.607, .018}, {.071, .057, .070}));
    merge(capsuleSignedDistance(p, {0, 1.696, .075}, {0, 1.666, .112}, .017));
    merge(ellipsoidSignedDistance(p, {.087, 1.681, -.002}, {.023, .039, .022}));
    merge(capsuleSignedDistance(p, {.095, .94, 0}, {.103, .54, .006}, .079));
    merge(capsuleSignedDistance(p, {.103, .54, .006}, {.102, .12, -.007}, .050));
    merge(ellipsoidSignedDistance(p, {.102, .055, .067}, {.061, .055, .137}));
    merge(capsuleSignedDistance(p, {.164, 1.422, 0}, {.432, 1.25, 0}, .059));
    merge(capsuleSignedDistance(p, {.432, 1.25, 0}, {.641, 1.066, 0}, std::max(.038, sampleRadius)));
    merge(capsuleSignedDistance(p, {.641, 1.066, 0}, {.702, 1.005, 0}, std::max(.032, sampleRadius)));
    for (const auto &f : fingers())
        d = smoothUnionDistance(
            d, capsuleSignedDistance(p, f.start, f.end, std::max(f.radius, sampleRadius)), .006);
    // Eye socket and mouth are actual recesses in the closed skin surface.
    d = std::max(d, -ellipsoidSignedDistance(p, {.033, 1.712, .088}, {.025, .012, .022}));
    d = std::max(d, -ellipsoidSignedDistance(p, {0, 1.635, .087}, {.033, .0035, .013}));
    return d;
}
double gaussian(Vector3 p, Vector3 center, Vector3 radius)
{
    p = sub(p, center);
    return std::exp(-.5 * (p.x * p.x / (radius.x * radius.x) + p.y * p.y / (radius.y * radius.y) +
                           p.z * p.z / (radius.z * radius.z)));
}
void range(double value, double low, double high)
{
    if (!std::isfinite(value) || value < low || value > high)
        throw std::invalid_argument("Body parameter outside supported range");
}
void validate(const BodyParameters &p, const GenerationOptions &o)
{
    range(p.height, 1.2, 2.4);
    range(p.legRatio, .42, .60);
    range(p.shoulderWidth, .28, .6);
    range(p.hipWidth, .22, .52);
    range(p.waistWidth, .18, .52);
    range(p.bodyDepth, .14, .4);
    range(p.armLength, .42, .85);
    range(p.headScale, .75, 1.35);
    range(p.noseScale, .5, 1.8);
    range(p.jawWidth, .7, 1.4);
    range(p.eyeSpacing, .7, 1.3);
    if (o.resolution < 32 || o.resolution > 160)
        throw std::invalid_argument("Generation resolution must be in [32,160]");
    if(o.skinBinding!=GeneratedSkinBinding::Envelope&&o.skinBinding!=GeneratedSkinBinding::Surface)
        throw std::invalid_argument("Unknown generated skin binding method");
}
Vector3 shapePoint(Vector3 p, const BodyParameters &s)
{
    const auto original = p;
    const auto shoulder = std::exp(-std::pow((p.y - 1.40) / .18, 2));
    const auto hip = std::exp(-std::pow((p.y - .94) / .16, 2));
    const auto waist = std::exp(-std::pow((p.y - 1.15) / .13, 2));
    p.x *= 1 + shoulder * (s.shoulderWidth / .40 - 1) + hip * (s.hipWidth / .32 - 1) +
           waist * (s.waistWidth / .28 - 1);
    p.z *= 1 + std::exp(-std::pow((p.y - 1.18) / .33, 4)) * (s.bodyDepth / .22 - 1);
    if (original.y > .80)
    {
        const auto arm = std::clamp((std::abs(original.x) - .14) / .12, 0.0, 1.0);
        const auto x = std::copysign(
            .20 * s.shoulderWidth / .40 + (std::abs(original.x) - .20) * s.armLength / .62, original.x);
        p.x = std::lerp(p.x, x, arm);
        p.y += arm * (original.y - 1.44) * (s.armLength / .62 - 1);
    }
    const auto head = std::clamp((original.y - 1.50) / .09, 0.0, 1.0);
    p = add(p, mul(sub(original, {0, 1.59, 0}), head * (s.headScale - 1)));
    p.x *= 1 + (s.jawWidth - 1) * gaussian(original, {0, 1.61, .03}, {.09, .035, .08});
    p.x *= 1 + (s.eyeSpacing - 1) * gaussian(original, {0, 1.713, .085}, {.07, .022, .035});
    p.z += (s.noseScale - 1) * .035 * gaussian(original, {0, 1.675, .105}, {.024, .035, .04});
    const auto legHeight = s.height * s.legRatio;
    p.y = p.y < .936 ? p.y * legHeight / .936 : legHeight + (p.y - .936) * (s.height - legHeight) / .864;
    // Uniform width/depth height scale keeps the proportions meaningful at different stature.
    p.x *= s.height / 1.8;
    p.z *= s.height / 1.8;
    return p;
}
// Six consistent tetrahedra per voxel, welded by grid-edge identity. Only +X is meshed;
// reflecting it produces exact bilateral correspondence and shares the centre-plane vertices.
SkeletalMeshLod surface(unsigned resolution)
{
    const auto h = 1.92 / resolution;
    ImplicitSurfaceOptions sampling;
    sampling.minimum = {0, -.04, -.28};
    sampling.maximum = {1.05, 1.88, .36};
    sampling.cells = {static_cast<unsigned>(std::ceil(1.05 / h)), resolution,
                      static_cast<unsigned>(std::ceil(.64 / h))};
    sampling.mirrorX = true;
    return ImplicitSurfaceMesher::generate([h](Vector3 p) { return humanField(p, .9 * h); }, sampling);
}
struct RigBuilder final
{
    CharacterData &data;
    const BodyParameters &parameters;
    std::vector<Vector3> world;
    std::vector<BoneEnvelope> envelopes;
    BoneIndex addJoint(std::string name, BoneIndex parent, Vector3 p, Vector3 end, double radius,
                       bool bind = true)
    {
        const auto position = shapePoint(p, parameters), target = shapePoint(end, parameters);
        const auto id = data.bones.size();
        data.bones.push_back(
            {name, parent, {parent == NoBone ? position : sub(position, world[parent]), {}, {1, 1, 1}}});
        data.semantics.push_back({std::move(name), id});
        world.push_back(position);
        if (bind)
            envelopes.push_back({id, position, target, radius * parameters.height / 1.8});
        return id;
    }
};
void buildRig(RigBuilder &r)
{
    const auto root = r.addJoint("root", NoBone, {}, {}, .1, false);
    auto spine = r.addJoint("pelvis", root, {0, .936, 0}, {0, 1.005, 0}, .12);
    for (unsigned i = 1; i <= 5; ++i)
    {
        const auto y = 1.005 + (i - 1) * .095;
        spine = r.addJoint("spine_0" + std::to_string(i), spine, {0, y, 0}, {0, y + .095, 0}, .12);
    }
    const auto neck = r.addJoint("neck_01", spine, {0, 1.50, 0}, {0, 1.56, 0}, .05);
    const auto neck2 = r.addJoint("neck_02", neck, {0, 1.56, 0}, {0, 1.65, 0}, .05);
    const auto head = r.addJoint("head", neck2, {0, 1.65, 0}, {0, 1.77, 0}, .07);
    r.addJoint("jaw", head, {0, 1.625, .012}, {0, 1.61, .072}, .027);
    r.addJoint("eye_l", head, {.033, 1.712, .079}, {.033, 1.712, .099}, .012, false);
    r.addJoint("eye_r", head, {-.033, 1.712, .079}, {-.033, 1.712, .099}, .012, false);
    for (int side : {1, -1})
    {
        const std::string suffix = side == 1 ? "_l" : "_r";
        const auto p = [&](double x, double y, double z = 0) { return Vector3{x * side, y, z}; };
        const auto clav = r.addJoint("clavicle" + suffix, spine, p(.11, 1.44), p(.20, 1.42), .06);
        const auto upper = r.addJoint("upperarm" + suffix, clav, p(.20, 1.42), p(.432, 1.25), .055);
        const auto lower = r.addJoint("lowerarm" + suffix, upper, p(.432, 1.25), p(.641, 1.066), .036);
        const auto hand = r.addJoint("hand" + suffix, lower, p(.641, 1.066), p(.702, 1.005), .03);
        for (unsigned i = 1; i <= 2; ++i)
        {
            const double t = i / 3.0;
            r.addJoint("upperarm_twist_0" + std::to_string(i) + suffix, upper,
                       p(std::lerp(.20, .432, t), std::lerp(1.42, 1.25, t)),
                       p(std::lerp(.20, .432, t + .2), std::lerp(1.42, 1.25, t + .2)), .035, false);
            r.addJoint("lowerarm_twist_0" + std::to_string(i) + suffix, lower,
                       p(std::lerp(.432, .641, t), std::lerp(1.25, 1.066, t)),
                       p(std::lerp(.432, .641, t + .2), std::lerp(1.25, 1.066, t + .2)), .026, false);
        }
        const std::string fingerNames[]{"thumb", "index", "middle", "ring", "pinky"};
        unsigned f = 0;
        for (auto finger : fingers())
        {
            auto parent = hand;
            for (unsigned k = 0; k < 3; ++k)
            {
                auto a = add(finger.start, mul(sub(finger.end, finger.start), k / 3.0));
                auto b = add(finger.start, mul(sub(finger.end, finger.start), (k + 1) / 3.0));
                a.x *= side;
                b.x *= side;
                parent =
                    r.addJoint(fingerNames[f] + "_0" + std::to_string(k + 1) + suffix, parent, a, b, .008);
            }
            ++f;
        }
        const auto thigh = r.addJoint("thigh" + suffix, 1, p(.095, .92), p(.103, .54, .006), .073);
        const auto calf = r.addJoint("calf" + suffix, thigh, p(.103, .54, .006), p(.102, .12, -.007), .043);
        const auto foot = r.addJoint("foot" + suffix, calf, p(.102, .12, -.007), p(.102, .055, .12), .045);
        r.addJoint("ball" + suffix, foot, p(.102, .055, .12), p(.102, .04, .19), .04);
        for (unsigned i = 1; i <= 2; ++i)
        {
            const auto t = i / 3.0;
            r.addJoint("thigh_twist_0" + std::to_string(i) + suffix, thigh,
                       p(std::lerp(.095, .103, t), std::lerp(.92, .54, t)),
                       p(std::lerp(.095, .103, t + .2), std::lerp(.92, .54, t + .2)), .05, false);
        }
    }
}
const std::array<std::string_view, 52> faceNames{
    "eyeBlinkLeft",      "eyeLookDownLeft",    "eyeLookInLeft",
    "eyeLookOutLeft",    "eyeLookUpLeft",      "eyeSquintLeft",
    "eyeWideLeft",       "eyeBlinkRight",      "eyeLookDownRight",
    "eyeLookInRight",    "eyeLookOutRight",    "eyeLookUpRight",
    "eyeSquintRight",    "eyeWideRight",       "jawForward",
    "jawLeft",           "jawRight",           "jawOpen",
    "mouthClose",        "mouthFunnel",        "mouthPucker",
    "mouthLeft",         "mouthRight",         "mouthSmileLeft",
    "mouthSmileRight",   "mouthFrownLeft",     "mouthFrownRight",
    "mouthDimpleLeft",   "mouthDimpleRight",   "mouthStretchLeft",
    "mouthStretchRight", "mouthRollLower",     "mouthRollUpper",
    "mouthShrugLower",   "mouthShrugUpper",    "mouthPressLeft",
    "mouthPressRight",   "mouthLowerDownLeft", "mouthLowerDownRight",
    "mouthUpperUpLeft",  "mouthUpperUpRight",  "browDownLeft",
    "browDownRight",     "browInnerUp",        "browOuterUpLeft",
    "browOuterUpRight",  "cheekPuff",          "cheekSquintLeft",
    "cheekSquintRight",  "noseSneerLeft",      "noseSneerRight",
    "tongueOut"};
Vector3 faceDelta(Vector3 p, std::string_view name)
{
    const double side = name.ends_with("Left") ? 1 : name.ends_with("Right") ? -1 : 0;
    const auto eye = gaussian(p, {side * .033, 1.712, .082}, {.026, .018, .04});
    const auto lip = gaussian(p, {side * .025, 1.635, .082}, {.028, .017, .034});
    if (name.starts_with("eye"))
    {
        if (name.starts_with("eyeBlink") || name.starts_with("eyeSquint"))
            return {0, (1.712 - p.y) * eye * (name.starts_with("eyeBlink") ? .9 : .5), .001 * eye};
        if (name.starts_with("eyeWide"))
            return {0, (p.y - 1.712) * eye * .8, 0};
        // Skin surrounding the eye follows a small amount of the gaze displacement.
        Vector3 direction;
        if (name.starts_with("eyeLookDown"))
            direction.y = -.002;
        if (name.starts_with("eyeLookUp"))
            direction.y = .002;
        if (name.starts_with("eyeLookIn"))
            direction.x = -side * .002;
        if (name.starts_with("eyeLookOut"))
            direction.x = side * .002;
        return mul(direction, eye);
    }
    if (name.starts_with("jaw"))
    {
        const auto w = gaussian(p, {0, 1.602, .035}, {.073, .046, .075});
        if (name == "jawOpen")
            return {0, -.025 * w, .010 * w};
        if (name == "jawForward")
            return {0, 0, .014 * w};
        return {(name == "jawLeft" ? .014 : -.014) * w, 0, 0};
    }
    if (name.starts_with("brow"))
    {
        const auto w = gaussian(p, {side * .045, 1.741, .072}, {.03, .018, .04});
        return {0, w * (name.starts_with("browDown") ? -.010 : .014), 0};
    }
    if (name.starts_with("cheek"))
    {
        const auto w = gaussian(p, {side * .048, 1.668, .073}, {.047, .03, .04});
        return {side * .004 * w, name == "cheekPuff" ? 0 : .006 * w, name == "cheekPuff" ? .012 * w : 0};
    }
    if (name.starts_with("nose"))
        return {0, .008 * gaussian(p, {side * .019, 1.675, .095}, {.02, .02, .03}), 0};
    if (name == "tongueOut")
        return {}; // The tongue is a separate oral mesh, not skin displacement.
    if (name == "mouthLeft" || name == "mouthRight")
        return {lip * (name == "mouthLeft" ? .012 : -.012), 0, 0};
    if (name == "mouthClose")
        return {0, (1.635 - p.y) * lip * .85, 0};
    if (name == "mouthPucker" || name == "mouthFunnel")
        return {-p.x * lip * .3, 0, lip * (name == "mouthPucker" ? .012 : .008)};
    if (name.starts_with("mouthSmile"))
        return {side * .008 * lip, .012 * lip, 0};
    if (name.starts_with("mouthFrown"))
        return {side * .003 * lip, -.009 * lip, 0};
    if (name.starts_with("mouthDimple"))
        return {side * .008 * lip, 0, -.004 * lip};
    if (name.starts_with("mouthStretch"))
        return {side * .014 * lip, 0, 0};
    if (name.starts_with("mouthPress"))
        return {0, (1.635 - p.y) * lip * .7, -.002 * lip};
    const bool lower = name.find("Lower") != std::string_view::npos;
    const auto vertical = gaussian(p, {side * .015, lower ? 1.627 : 1.643, .084}, {.03, .009, .03});
    if (name.starts_with("mouthRoll"))
        return {0, vertical * (lower ? .003 : -.003), -.005 * vertical};
    if (name.starts_with("mouthShrug"))
        return {0, vertical * (lower ? .005 : -.005), .003 * vertical};
    return {0, vertical * (lower ? -.012 : .009), 0};
}
CharacterMesh eyeMesh(bool left, BoneIndex bone, const BodyParameters &parameters)
{
    auto eye = MeshPrimitives::ellipsoid({left ? .033 : -.033, 1.712, .079}, {.023, .013, .018});
    eye.name = left ? "EyeLeft" : "EyeRight";
    eye.material = 1;
    for (auto &v : eye.geometry.vertices)
    {
        v.position = shapePoint(v.position, parameters);
        v.weights = {{bone, 1}};
    }
    recomputeNormals(eye.geometry);
    return eye;
}
CharacterMesh tongueMesh(BoneIndex jaw, std::size_t material, const BodyParameters &parameters)
{
    auto tongue=MeshPrimitives::ellipsoid({0,1.632,.074},{.024,.0027,.020},8,16);
    tongue.name="Tongue";tongue.material=material;
    std::vector<Vector3> canonical;canonical.reserve(tongue.geometry.vertices.size());
    for(auto &vertex:tongue.geometry.vertices)
    {
        canonical.push_back(vertex.position);vertex.position=shapePoint(vertex.position,parameters);
        vertex.weights={{jaw,1}};
    }
    recomputeNormals(tongue.geometry);
    for(const auto name:std::array<std::string_view,5>{"tongueOut","jawOpen","jawForward","jawLeft","jawRight"})
    {
        auto endpoint=tongue.geometry;endpoint.morphTargets.clear();MorphTarget target{std::string(name),{}};
        for(std::size_t i=0;i<canonical.size();++i)
        {
            Vector3 delta;
            if(name=="tongueOut")
            {
                const auto forward=std::clamp((canonical[i].z-.054)/.040,0.,1.);
                delta={0,-.004*forward,.040*forward};
            }
            else if(name=="jawOpen")delta={0,-.018,.006};
            else if(name=="jawForward")delta={0,0,.012};
            else delta={name=="jawLeft"?.010:-.010,0,0};
            const auto position=shapePoint(add(canonical[i],delta),parameters);
            endpoint.vertices[i].position=position;
            target.deltas.push_back({i,sub(position,tongue.geometry.vertices[i].position),{}});
        }
        recomputeNormals(endpoint);
        for(auto &delta:target.deltas)
            delta.normal=sub(endpoint.vertices[delta.vertex].normal,tongue.geometry.vertices[delta.vertex].normal);
        tongue.geometry.morphTargets.push_back(std::move(target));
    }
    return tongue;
}
} // namespace
std::shared_ptr<const CharacterAsset> HumanoidGenerator::generate(const Object3DReference &engine,
                                                                  const BodyParameters &parameters,
                                                                  const GenerationOptions &options)
{
    if (!engine.valid())
        throw std::logic_error("Humanoid generation requires a live core 3D engine");
    validate(parameters, options);
    CharacterData data;
    data.name = "GeneratedHuman";
    RigBuilder rig{data, parameters, {}, {}};
    buildRig(rig);
    CharacterMesh body;
    body.name = "Body";
    body.geometry = surface(options.resolution);
    std::vector<Vector3> canonical;
    for (auto &v : body.geometry.vertices)
    {
        canonical.push_back(v.position);
        body.textureCoordinates.push_back({.5 + v.position.x / 2.1, v.position.y / 1.8});
        v.position = shapePoint(v.position, parameters);
    }
    if (options.facialMorphs)
        for (const auto name : faceNames)
        {
            MorphTarget target{std::string(name), {}};
            for (std::size_t i = 0; i < canonical.size(); ++i)
            {
                const auto delta = faceDelta(canonical[i], name);
                if (length(delta) > 1e-7)
                    target.deltas.push_back({i,
                                             sub(shapePoint(add(canonical[i], delta), parameters),
                                                 body.geometry.vertices[i].position),
                                             {}});
            }
            body.geometry.morphTargets.push_back(std::move(target));
        }
    recomputeNormals(body.geometry);
    data.meshes.push_back(std::move(body));
    if(options.skinBinding==GeneratedSkinBinding::Surface)
        (void)AutoRigger::bindSurfaceMesh(data,0,rig.envelopes,options.surfaceBinding);
    else AutoRigger::bind(data, rig.envelopes, {4, 6});
    if (options.eyes)
    {
        data.materials.push_back({"Eyes", {.8, .84, .85, 1}, 0, .2,{}, {}});
        BoneIndex left = NoBone, right = NoBone;
        for (const auto &s : data.semantics)
        {
            if (s.name == "eye_l")
                left = s.bone;
            if (s.name == "eye_r")
                right = s.bone;
        }
        data.meshes.push_back(eyeMesh(true, left, parameters));
        data.meshes.push_back(eyeMesh(false, right, parameters));
    }
    if(options.facialMorphs)
    {
        const auto jaw=std::find_if(data.semantics.begin(),data.semantics.end(),[](const auto &semantic){return semantic.name=="jaw";});
        if(jaw==data.semantics.end())throw std::logic_error("Generated tongue requires a jaw joint");
        const auto material=data.materials.size();Object3DMaterial oral;
        oral.name="Tongue";oral.baseColor={.36,.08,.11,1};oral.roughness=.55;
        data.materials.push_back(std::move(oral));
        data.meshes.push_back(tongueMesh(jaw->bone,material,parameters));
    }
    return std::make_shared<CharacterAsset>(engine.createObject(std::move(data)));
}
std::shared_ptr<const CharacterAsset> HumanoidGenerator::generate(Object3DEngine &engine,
                                                                  const BodyParameters &parameters,
                                                                  const GenerationOptions &options)
{
    return generate(engine.reference(), parameters, options);
}
} // namespace iiActorObject::NSCharacter
