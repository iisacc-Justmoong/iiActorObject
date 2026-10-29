#include "Object3DTemplate.h"
#include "Object3DTemplateJointFrames.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <limits>
#include <set>
#include <stdexcept>

namespace iiActorObject
{
namespace
{
Vector3 add(Vector3 a,Vector3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vector3 sub(Vector3 a,Vector3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vector3 mul(Vector3 a,double s) { return {a.x*s,a.y*s,a.z*s}; }
double dot(Vector3 a,Vector3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vector3 cross(Vector3 a,Vector3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
bool finite(Vector3 a) { return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z); }
Vector3 unit(Vector3 a)
{
    const auto n=std::hypot(a.x,a.y,a.z);
    if(!std::isfinite(n)||n<1e-12)throw std::invalid_argument("Degenerate template joint direction");
    return mul(a,1/n);
}
Quaternion product(Quaternion a,Quaternion b)
{
    return Quaternion{a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
        a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
        a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z}.normalized();
}
Quaternion inverse(Quaternion q) { return {-q.x,-q.y,-q.z,q.w}; }
Quaternion frame(Vector3 direction,Vector3 hint,Vector3 defaultHint,Vector3 fallbackHint,double relativeParallelTolerance)
{
    const auto y=unit(direction);
    auto z=sub(hint,mul(y,dot(y,hint)));
    const auto parallelLimit=std::max(1e-10,relativeParallelTolerance*std::hypot(hint.x,hint.y,hint.z));
    if(std::hypot(z.x,z.y,z.z)<parallelLimit)
    {
        hint=std::abs(dot(y,unit(defaultHint)))<.9?defaultHint:fallbackHint;
        z=sub(hint,mul(y,dot(y,hint)));
    }
    z=unit(z);const auto x=unit(cross(y,z));z=cross(x,y);
    const double m00=x.x,m01=y.x,m02=z.x,m10=x.y,m11=y.y,m12=z.y,m20=x.z,m21=y.z,m22=z.z;
    Quaternion q;const auto trace=m00+m11+m22;
    if(trace>0) { const auto s=2*std::sqrt(trace+1);q={(m21-m12)/s,(m02-m20)/s,(m10-m01)/s,s/4}; }
    else if(m00>m11&&m00>m22) { const auto s=2*std::sqrt(1+m00-m11-m22);q={s/4,(m01+m10)/s,(m02+m20)/s,(m21-m12)/s}; }
    else if(m11>m22) { const auto s=2*std::sqrt(1+m11-m00-m22);q={(m01+m10)/s,s/4,(m12+m21)/s,(m02-m20)/s}; }
    else { const auto s=2*std::sqrt(1+m22-m00-m11);q={(m02+m20)/s,(m12+m21)/s,s/4,(m10-m01)/s}; }
    return q.normalized();
}
void validateAnchor(const TemplateAnchor &a,const Object3DData &d)
{
    if(a.mesh>=d.meshes.size()||a.points.empty()||a.points.size()>4096)
        throw std::invalid_argument("Invalid template anchor");
    const auto &mesh=d.meshes[a.mesh];const auto count=mesh.topology?mesh.topology->points.size():mesh.geometry.vertices.size();
    std::set<std::uint32_t> used;
    for(const auto p:a.points)
        if(p>=count||!used.insert(p).second)
            throw std::invalid_argument("Invalid or duplicate anchor source point");
}
Vector3 anchor(const TemplateAnchor &a,const Object3DData &d)
{
    Vector3 result{};const auto &mesh=d.meshes[a.mesh];
    for(const auto p:a.points)
        result=add(result,mul(mesh.topology?mesh.topology->points[p].position:mesh.geometry.vertices[p].position,
                              1./static_cast<double>(a.points.size())));
    if(!finite(result))throw std::invalid_argument("Template anchor overflow");return result;
}
double curve(const TemplateControlFactor &factor,double value)
{
    const auto next=std::upper_bound(factor.knots.begin(),factor.knots.end(),value,
        [](double input,const TemplateControlKnot &key){return input<key.value;});
    if(next==factor.knots.begin())return next->weight;
    if(next==factor.knots.end())return factor.knots.back().weight;
    const auto previous=std::prev(next);
    return std::lerp(previous->weight,next->weight,(value-previous->value)/(next->value-previous->value));
}
void validateControls(const Object3DTemplateDefinition &definition)
{
    if(definition.controls.size()>4096||definition.drivers.size()>4096)throw std::length_error("Template control/driver limit");
    std::map<std::string_view,const TemplateControl *> controls;std::set<std::string_view> names;
    for(const auto &target:definition.targets)names.insert(target.name);
    const auto text=[](const std::string &value,bool empty){return (empty||!value.empty())&&value.size()<=4096&&value.find('\0')==std::string::npos;};
    for(const auto &control:definition.controls)
    {
        if(!text(control.name,false)||!text(control.label,true)||!text(control.group,true)||!names.insert(control.name).second||
           !std::isfinite(control.minimum)||!std::isfinite(control.maximum)||control.minimum>0||control.maximum<0||
           control.minimum>=control.maximum||control.minimum< -1e6||control.maximum>1e6)throw std::invalid_argument("Invalid template control definition");
        controls.emplace(control.name,&control);
    }
    std::set<std::size_t> targets;std::size_t knots=0;
    for(const auto &driver:definition.drivers)
    {
        if(driver.target>=definition.targets.size()||!targets.insert(driver.target).second||!std::isfinite(driver.scale)||
           std::abs(driver.scale)>1e6||driver.factors.empty()||driver.factors.size()>8)throw std::invalid_argument("Invalid template target driver");
        std::set<std::string_view> inputs;double minimum=1,maximum=1,neutral=1;
        for(const auto &factor:driver.factors)
        {
            const auto found=controls.find(factor.control);
            if(found==controls.end()||!inputs.insert(factor.control).second||factor.knots.size()<2||factor.knots.size()>64)
                throw std::invalid_argument("Invalid template driver input");
            const auto &control=*found->second;
            if(factor.knots.front().value!=control.minimum||factor.knots.back().value!=control.maximum)
                throw std::invalid_argument("Template control curve must cover its whole range");
            if(factor.knots.size()>1'048'576-knots)throw std::length_error("Template control knot budget");knots+=factor.knots.size();
            double low=std::numeric_limits<double>::max(),high=std::numeric_limits<double>::lowest();
            for(std::size_t i=0;i<factor.knots.size();++i)
            {
                const auto key=factor.knots[i];
                if(!std::isfinite(key.value)||!std::isfinite(key.weight)||std::abs(key.weight)>1e6||
                   (i&&key.value<=factor.knots[i-1].value))throw std::invalid_argument("Invalid template control curve key");
                low=std::min(low,key.weight);high=std::max(high,key.weight);
            }
            const std::array products{minimum*low,minimum*high,maximum*low,maximum*high};
            minimum=*std::min_element(products.begin(),products.end());maximum=*std::max_element(products.begin(),products.end());neutral*=curve(factor,0);
        }
        const auto a=driver.scale*(minimum-neutral),b=driver.scale*(maximum-neutral);const auto &target=definition.targets[driver.target];
        if(!std::isfinite(a)||!std::isfinite(b)||std::min(a,b)<target.minimum-1e-12||std::max(a,b)>target.maximum+1e-12)
            throw std::invalid_argument("Template driver exceeds the target weight domain");
    }
}
}
Object3DTemplate::Object3DTemplate(Object3DReference engine,Object3DTemplateDefinition definition)
    :engine_(std::move(engine)),definition_(std::move(definition))
{
    const auto &d=definition_.base.get().data();std::set<std::string> names;
    if(definition_.targets.size()>4096)throw std::length_error("Template target count limit");
    std::size_t deltas=0;
    for(const auto &t:definition_.targets)
    {
        if(t.name.empty()||!names.insert(t.name).second||t.mesh>=d.meshes.size()||!d.meshes[t.mesh].topology||
           !std::isfinite(t.minimum)||!std::isfinite(t.maximum)||t.minimum>0||t.maximum<0||t.minimum>=t.maximum)
            throw std::invalid_argument("Invalid template target definition");
        if(t.deltas.size()>16'000'000-deltas)throw std::length_error("Template sparse delta limit");deltas+=t.deltas.size();
        std::set<std::uint32_t> points;
        for(const auto &v:t.deltas)
            if(v.point>=d.meshes[t.mesh].topology->points.size()||!finite(v.position)||!points.insert(v.point).second)
                throw std::invalid_argument("Invalid template target point");
    }
    validateControls(definition_);
    if(definition_.poseProgram)validateObject3DPoseProgram(*definition_.poseProgram);
    std::set<BoneIndex> bones;
    for(const auto &j:definition_.joints)
    {
        if(j.bone>=d.bones.size()||!bones.insert(j.bone).second)throw std::invalid_argument("Invalid template joint binding");
        validateAnchor(j.head,d);validateAnchor(j.tail,d);
        if(j.rotationPlane)for(const auto &a:*j.rotationPlane)validateAnchor(a,d);
    }
    if(!definition_.joints.empty())
        for(const auto &b:d.bones)
            if(b.bindTransform.scale!=Vector3{1,1,1})throw std::invalid_argument("Template joint adaptation requires unit bind scale");
    validateMeshSurfaceBindingsWithBases(d,definition_.surfaces,definition_.surfaceOffsetBases);
    if(std::none_of(definition_.surfaceOffsetBases.begin(),definition_.surfaceOffsetBases.end(),[](const auto &basis){return basis.has_value();}))
        definition_.surfaceOffsetBases.clear();
    auto neutral=evaluate({});
    (void)applyObject3DSurfaceFittingWithBases(neutral,definition_.surfaces,definition_.fittings,definition_.surfaceOffsetBases);
}
const Object3DTemplateDefinition &Object3DTemplate::definition() const noexcept { return definition_; }
Vector3 groundObject3D(Object3DData &data,const TemplateGrounding &grounding)
{
    if(grounding.mesh>=data.meshes.size()||!finite(grounding.normal)||!std::isfinite(grounding.offset))
        throw std::invalid_argument("Invalid object grounding plane");
    const auto normal=unit(grounding.normal);const auto &mesh=data.meshes[grounding.mesh];
    double minimum=std::numeric_limits<double>::infinity();
    if(!grounding.supportPoints.empty())
    {
        const auto count=mesh.topology?mesh.topology->points.size():mesh.geometry.vertices.size();
        if(grounding.supportPoints.size()>count)throw std::invalid_argument("Invalid grounding support point count");
        std::vector<bool> seen(count);
        for(const auto point:grounding.supportPoints)
        {
            if(point>=count||seen[point])throw std::invalid_argument("Invalid or duplicate grounding support point");
            seen[point]=true;const auto position=mesh.topology?mesh.topology->points[point].position:mesh.geometry.vertices[point].position;
            minimum=std::min(minimum,dot(normal,position));
        }
    }
    else if(mesh.topology)
    {
        if(!mesh.topology->layout)throw std::invalid_argument("Grounding requires an authored layout");
        const auto &layout=mesh.topology->layout->definition();
        for(const auto vertex:layout.renderToLayout)
            minimum=std::min(minimum,dot(normal,mesh.topology->points.at(layout.layouts.at(vertex).position).position));
    }
    else for(const auto &vertex:mesh.geometry.vertices)minimum=std::min(minimum,dot(normal,vertex.position));
    const auto displacement=mul(normal,grounding.offset-minimum);
    if(!std::isfinite(minimum)||!finite(displacement))throw std::invalid_argument("Empty or nonfinite grounding surface");
    const auto validate=[&](Vector3 position){if(!finite(position)||!finite(add(position,displacement)))throw std::invalid_argument("Grounding position overflow");};
    for(const auto &part:data.meshes)
    {
        if(part.topology)for(const auto &point:part.topology->points)validate(point.position);
        for(const auto &vertex:part.geometry.vertices)validate(vertex.position);
    }
    for(const auto &bone:data.bones)if(bone.parent==NoBone)validate(bone.bindTransform.position);
    for(auto &part:data.meshes)
    {
        if(part.topology)for(auto &point:part.topology->points)point.position=add(point.position,displacement);
        for(auto &vertex:part.geometry.vertices)vertex.position=add(vertex.position,displacement);
    }
    for(auto &bone:data.bones)if(bone.parent==NoBone)bone.bindTransform.position=add(bone.bindTransform.position,displacement);
    return displacement;
}
std::vector<double> Object3DTemplate::resolveValues(std::span<const TemplateValue> values) const
{
    if(values.size()>definition_.targets.size()+definition_.controls.size())throw std::invalid_argument("Template input count exceeds catalog");
    std::map<std::string_view,std::size_t> targets;std::map<std::string_view,std::pair<const TemplateControl *,double>> controls;
    for(std::size_t i=0;i<definition_.targets.size();++i)targets.emplace(definition_.targets[i].name,i);
    for(const auto &control:definition_.controls)controls.emplace(control.name,std::pair{&control,0.});
    std::vector<double> weights(definition_.targets.size(),0);std::set<std::string_view> seen;
    for(const auto &value:values)
    {
        if(!seen.insert(value.name).second||!std::isfinite(value.value))throw std::invalid_argument("Duplicate or nonfinite template input");
        if(const auto target=targets.find(value.name);target!=targets.end())
        {
            const auto &definition=definition_.targets[target->second];
            if(value.value<definition.minimum||value.value>definition.maximum)throw std::invalid_argument("Template target input range");
            weights[target->second]=value.value;
        }
        else if(const auto control=controls.find(value.name);control!=controls.end())
        {
            if(value.value<control->second.first->minimum||value.value>control->second.first->maximum)throw std::invalid_argument("Template control input range");
            control->second.second=value.value;
        }
        else throw std::invalid_argument("Unknown template input");
    }
    for(const auto &driver:definition_.drivers)
    {
        double value=1,neutral=1;
        for(const auto &factor:driver.factors){value*=curve(factor,controls.at(factor.control).second);neutral*=curve(factor,0);}
        weights[driver.target]+=driver.scale*(value-neutral);
    }
    for(std::size_t i=0;i<weights.size();++i)
        if(!std::isfinite(weights[i])||weights[i]<definition_.targets[i].minimum||weights[i]>definition_.targets[i].maximum)
            throw std::invalid_argument("Combined template weight exceeds target range");
    return weights;
}
void bindObject3DTemplateJoints(Object3DData &data,std::span<const TemplateJointBinding> joints)
{
    if(joints.empty())return;
    if(joints.size()>data.bones.size())throw std::invalid_argument("Invalid template joint binding count");
    std::vector<TemplateJointFrame> frames;frames.reserve(joints.size());
    for(const auto &j:joints)
    {
        validateAnchor(j.head,data);validateAnchor(j.tail,data);
        if(j.rotationPlane)for(const auto &a:*j.rotationPlane)validateAnchor(a,data);
        TemplateJointFrame f{j.bone,anchor(j.head,data),anchor(j.tail,data),{}};
        if(j.rotationPlane)f.rotationPlane=std::array{anchor((*j.rotationPlane)[0],data),
            anchor((*j.rotationPlane)[1],data),anchor((*j.rotationPlane)[2],data)};
        frames.push_back(std::move(f));
    }
    bindObject3DJointFrames(data,frames);
}
void object3d_detail::bindJointFrames(Object3DData &data,std::span<const TemplateJointFrame> joints,
                                      Vector3 defaultHint,Vector3 fallbackHint,double relativeParallelTolerance)
{
    if(joints.empty())return;
    if(!finite(defaultHint)||!finite(fallbackHint)||std::hypot(defaultHint.x,defaultHint.y,defaultHint.z)==0||
       std::hypot(fallbackHint.x,fallbackHint.y,fallbackHint.z)==0||!std::isfinite(relativeParallelTolerance)||
       relativeParallelTolerance<0||relativeParallelTolerance>=1)
        throw std::invalid_argument("Invalid template default frame hint");
    const Skeleton skeleton(data.bones);
    if(joints.size()>data.bones.size())throw std::invalid_argument("Invalid template joint frame count");
    std::vector<const TemplateJointFrame *> bindings(data.bones.size());
    for(const auto &j:joints)
    {
        if(j.bone>=bindings.size()||bindings[j.bone]||!finite(j.head)||!finite(j.tail))
            throw std::invalid_argument("Invalid template joint frame");
        if(j.rotationPlane)for(const auto p:*j.rotationPlane)if(!finite(p))throw std::invalid_argument("Invalid template rotation plane");
        bindings[j.bone]=&j;
    }
    auto bones=data.bones;
    std::vector<Matrix4> world(bones.size());std::vector<Quaternion> rotations(bones.size());
    for(const auto i:skeleton.evaluationOrder())
    {
        auto &bone=bones[i];const auto parent=bone.parent;
        if(bone.bindTransform.scale!=Vector3{1,1,1})throw std::invalid_argument("Template joint adaptation requires unit bind scale");
        const auto parentWorld=parent==NoBone?Matrix4{}:world[parent];
        const auto parentRotation=parent==NoBone?Quaternion{}:rotations[parent];
        if(const auto j=bindings[i])
        {
            const auto h=j->head,tail=j->tail;Vector3 hint=defaultHint;
            if(j->rotationPlane)
            {
                const auto a=(*j->rotationPlane)[0],b=(*j->rotationPlane)[1],c=(*j->rotationPlane)[2];
                hint=cross(sub(b,a),sub(c,a));
            }
            // Preserve a deliberately authored rotation plane. The relaxed
            // parallel test only stabilizes the implicit default reference.
            const auto rotation=frame(sub(tail,h),hint,defaultHint,fallbackHint,
                                      j->rotationPlane ? 0. : relativeParallelTolerance);
            bone.bindTransform={parentWorld.invertedAffine().transformPoint(h),product(inverse(parentRotation),rotation),{1,1,1}};
        }
        rotations[i]=product(parentRotation,bone.bindTransform.rotation);
        world[i]=parentWorld*bone.bindTransform.matrix();
    }
    data.bones=std::move(bones);
}
void bindObject3DJointFrames(Object3DData &data,std::span<const TemplateJointFrame> joints)
{
    object3d_detail::bindJointFrames(data,joints,{0,0,1},{1,0,0},0);
}
Object3DData Object3DTemplate::evaluate(std::span<const TemplateValue> values) const
{
    const auto weights=resolveValues(values);
    auto d=definition_.base.get().data();std::vector<bool> moved(d.meshes.size(),false);
    for(std::size_t i=0;i<weights.size();++i)
    {
        if(weights[i]==0)continue;const auto &target=definition_.targets[i];auto &points=d.meshes[target.mesh].topology->points;
        for(const auto &delta:target.deltas)
        {
            auto &p=points[delta.point].position;const auto change=mul(delta.position,weights[i]);p=add(p,change);
            if(change!=Vector3{})moved[target.mesh]=true;
            if(!finite(p))throw std::invalid_argument("Template shape overflow");
        }
    }
    applyMeshSurfaceBindingsWithBases(d,definition_.base.get().data(),definition_.surfaces,definition_.surfaceOffsetBases);
    bindObject3DTemplateJoints(d,definition_.joints);
    if(definition_.grounding)(void)groundObject3D(d,*definition_.grounding);
    for(std::size_t i=0;i<d.meshes.size();++i)if(moved[i]&&d.meshes[i].topology)recomputeTopologyNormals(*d.meshes[i].topology);
    return d;
}
Object3DHandle Object3DTemplate::generate(std::span<const TemplateValue> values,std::string name) const
{
    auto d=evaluateFinal(values);
    if(!name.empty())d.name=std::move(name);return engine_.createObject(std::move(d));
}
Object3DData Object3DTemplate::evaluateFinal(std::span<const TemplateValue> values) const
{
    auto d=evaluate(values);
    if(!applyObject3DSurfaceFittingWithBases(d,definition_.surfaces,definition_.fittings,definition_.surfaceOffsetBases).empty())bindObject3DTemplateJoints(d,definition_.joints);
    return d;
}
Object3DData Object3DTemplate::snapshot(std::span<const TemplateValue> values) const
{
    auto d=evaluateFinal(values);
    for(auto &mesh:d.meshes)if(mesh.topology)
    {
        auto render=renderMeshTopology(*mesh.topology);
        mesh.geometry=std::move(render.geometry);mesh.textureCoordinates=std::move(render.textureCoordinates);
        mesh.triangleMaterials=std::move(render.triangleMaterials);mesh.primaryUvName=std::move(render.primaryUvName);
        mesh.additionalUvSets=std::move(render.additionalUvSets);
    }
    return d;
}
} // namespace iiActorObject
