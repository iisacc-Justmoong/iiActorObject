#include "Object3DFbxAnimationImport.h"
#include "Object3DProgressiveMorph.h"
#include "CharacterMath.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <numbers>
#include <numeric>
#include <set>
#include <stdexcept>
#include <unordered_set>

namespace iiActorObject::object3d_detail
{
namespace
{
std::string text(ufbx_string value)
{
    if(value.length>4096 || (!value.data && value.length))throw std::invalid_argument("Invalid FBX animation name");
    return {value.data?value.data:"",value.length};
}
std::string_view view(ufbx_string value)
{
    return {value.data?value.data:"",value.length};
}
bool transformProperty(std::string_view name)
{
    constexpr std::array names{std::string_view{"Lcl Translation"},std::string_view{"Lcl Rotation"},
        std::string_view{"Lcl Scaling"},std::string_view{"PreRotation"},std::string_view{"PostRotation"},
        std::string_view{"RotationOffset"},std::string_view{"RotationPivot"},
        std::string_view{"ScalingOffset"},std::string_view{"ScalingPivot"}};
    return std::find(names.begin(),names.end(),name)!=names.end();
}
void validateOptions(const FbxAnimationImportOptions &o)
{
    const auto positive=[](double v){return std::isfinite(v)&&v>0;};
    if(!positive(o.maximumSampleInterval) || !positive(o.maximumPositionError) ||
       !positive(o.maximumRotationError) || o.maximumRotationError>std::numbers::pi ||
       !positive(o.maximumScaleError) || !positive(o.maximumMorphError) ||
       o.maximumKeys==0 || o.maximumKeys>10000000 || o.maximumEvaluations==0 ||
       o.maximumEvaluations>100000 || o.maximumEvaluationValues==0 || o.maximumEvaluationValues>20000000 ||
       o.maximumClips==0 || o.maximumClips>1024 ||
       (o.morphGrouping!=FbxMorphAnimationGrouping::MergeByName&&o.morphGrouping!=FbxMorphAnimationGrouping::PerMesh))
        throw std::invalid_argument("Invalid FBX animation import limits");
}
Transform transform(const ufbx_matrix &matrix,double tolerance)
{
    const auto converted=ufbx_matrix_to_transform(&matrix);
    auto result=character_detail::skeletalTransform({{converted.translation.x,converted.translation.y,converted.translation.z},
        {converted.rotation.x,converted.rotation.y,converted.rotation.z,converted.rotation.w},
        {converted.scale.x,converted.scale.y,converted.scale.z}});
    const auto resultMatrix=result.matrix();const auto &check=resultMatrix.elements();
    for(std::size_t col=0;col<4;++col)for(std::size_t row=0;row<3;++row)
    {
        const auto value=matrix.v[col*3+row];
        const auto error=std::abs(check[col*4+row]-value)/std::max(1.,std::abs(value));
        if(!std::isfinite(error)||error>tolerance)
            throw std::invalid_argument("Animated FBX joint cannot be represented without shear");
    }
    return result;
}
double rotationError(Quaternion a,Quaternion b)
{
    a=a.normalized();b=b.normalized();
    if(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w<0)b={-b.x,-b.y,-b.z,-b.w};
    const auto d=std::hypot(std::hypot(a.x-b.x,a.y-b.y),std::hypot(a.z-b.z,a.w-b.w));
    return 4*std::asin(std::min(1.,d*.5));
}
double distance(Vector3 a,Vector3 b)
{
    return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);
}
struct MorphGroup final
{
    std::string name;
    std::vector<const ufbx_blend_channel *> channels;
    std::vector<std::pair<std::size_t,std::size_t>> targets;
    double maximumError=0;
};
double progressiveControlTolerance(const ufbx_blend_channel &channel,double maximumWeightError)
{
    double smallest=std::numeric_limits<double>::infinity(),previous=0;bool first=true;
    for(const auto &key:channel.keyframes)
    {
        const double weight=key.target_weight;
        if(!std::isfinite(weight)||weight==0||(!first&&weight<=previous))
            throw std::invalid_argument("Invalid FBX progressive animation weights");
        smallest=std::min(smallest,std::abs(weight));
        if(!first&&std::signbit(previous)==std::signbit(weight))smallest=std::min(smallest,weight-previous);
        previous=weight;first=false;
    }
    const auto tolerance=maximumWeightError*std::min(1.,smallest);
    if(first||!std::isfinite(tolerance)||!(tolerance>0))
        throw std::invalid_argument("Progressive animation tolerance cannot be represented");
    return tolerance;
}
struct Frame final
{
    std::vector<Transform> bones;
    std::vector<double> morphs;
};
struct Budget final
{
    std::size_t keys=0,evaluations=0,values=0;
};
void warn(std::vector<std::string> &warnings,std::string message)
{
    if(std::find(warnings.begin(),warnings.end(),message)==warnings.end())warnings.push_back(std::move(message));
}
void unsupported(const FbxAnimationImportOptions &options,std::vector<std::string> &warnings)
{
    if(!options.allowDiscardUnsupportedAnimation)
        throw std::invalid_argument("FBX has animation or constraints not represented by the skeletal and morph clip");
    warn(warnings,"Some FBX animation or constraints were explicitly discarded.");
}
struct SourceKeys final
{
    std::set<double> times;
    std::unordered_set<std::uint32_t> animatedMorphs;
};
SourceKeys sourceKeys(const ufbx_scene &scene,const ufbx_anim_stack &stack,
                      std::span<const FbxImportedNode> nodes,const FbxImportedMorphs &morphs,
                      std::span<const FbxImportedProgressiveControl> progressive,
                      const FbxAnimationImportOptions &options,std::vector<std::string> &warnings)
{
    std::unordered_set<std::uint32_t> ancestors,channels,layers;
    for(const auto &entry:nodes)
    {
        auto *node=entry.node;std::size_t count=0;
        while(node)
        {
            if(++count>scene.nodes.count)throw std::invalid_argument("FBX animation parent cycle");
            ancestors.insert(node->element_id);node=node->parent;
        }
    }
    for(const auto &morph:morphs)for(const auto *channel:morph.channels)channels.insert(channel->element_id);
    for(const auto &control:progressive)
    {
        if(control.channels.empty())throw std::invalid_argument("Missing FBX progressive animation channel");
        for(const auto *channel:control.channels)
        {
            if(!channel)throw std::invalid_argument("Missing FBX progressive animation channel");
            channels.insert(channel->element_id);
        }
    }
    for(const auto *layer:stack.layers)layers.insert(layer->element_id);
    SourceKeys result;result.times={0,stack.time_end-stack.time_begin};
    for(const auto *layer:stack.layers)for(const auto &prop:layer->anim_props)
    {
        if(!prop.element||!prop.anim_value)throw std::invalid_argument("Malformed FBX animated property");
        const auto property=view(prop.prop_name);bool supported=false;
        if(prop.element->type==UFBX_ELEMENT_NODE)
        {
            const auto *node=ufbx_as_node(prop.element);
            supported=node && !node->mesh && ancestors.contains(node->element_id) && transformProperty(property);
        }
        else if(prop.element->type==UFBX_ELEMENT_BLEND_CHANNEL)
        {
            supported=property=="DeformPercent" && channels.contains(prop.element->element_id);
            if(supported)result.animatedMorphs.insert(prop.element->element_id);
        }
        else if(prop.element->type==UFBX_ELEMENT_ANIM_LAYER)
            supported=property=="Weight" && layers.contains(prop.element->element_id);
        if(!supported){unsupported(options,warnings);continue;}
        for(const auto *curve:prop.anim_value->curves)
        {
            if(!curve)continue;
            const auto &keys=curve->keyframes;
            for(std::size_t i=0;i<keys.count;++i)
            {
                const auto &key=keys.data[i];
                if(!std::isfinite(key.time)||!std::isfinite(key.value))throw std::invalid_argument("Nonfinite FBX animation key");
                if(i && key.time<=keys.data[i-1].time)throw std::invalid_argument("FBX animation key times must increase");
                if(i+1<keys.count && key.time<stack.time_end && keys.data[i+1].time>stack.time_begin &&
                   (key.interpolation==UFBX_INTERPOLATION_CONSTANT_PREV || key.interpolation==UFBX_INTERPOLATION_CONSTANT_NEXT) &&
                   key.value!=keys.data[i+1].value)
                    throw std::invalid_argument("Discontinuous FBX animation cannot be represented by a continuous clip");
                if(key.time>stack.time_begin && key.time<stack.time_end)
                {
                    const double relative=key.time-stack.time_begin;
                    if(relative<=0 || relative>=stack.time_end-stack.time_begin)
                        throw std::invalid_argument("FBX key time loses precision relative to its take");
                    result.times.insert(relative);
                    if(result.times.size()>options.maximumKeys)throw std::length_error("FBX animation input key limit");
                }
            }
            if(keys.count>0 && ((stack.time_begin<keys.data[0].time &&
                                  curve->pre_extrapolation.mode!=UFBX_EXTRAPOLATION_CONSTANT) ||
                                 (stack.time_end>keys.data[keys.count-1].time &&
                                  curve->post_extrapolation.mode!=UFBX_EXTRAPOLATION_CONSTANT)))
                throw std::invalid_argument("Nonconstant FBX extrapolation requires explicit baking");
        }
    }
    return result;
}
std::vector<double> sampleTimes(const std::set<double> &source,double interval,std::size_t limit)
{
    if(limit<source.size())throw std::length_error("FBX animation key budget exceeded");
    std::vector<double> result;result.reserve(source.size());auto current=source.begin();result.push_back(*current++);
    for(;current!=source.end();++current)
    {
        const double end=*current,start=result.back(),gap=end-start;
        const double count=std::ceil(gap/interval);
        if(!std::isfinite(count)||count>static_cast<double>(limit-result.size()))
            throw std::length_error("FBX animation key budget exceeded");
        const auto pieces=std::max<std::size_t>(1,static_cast<std::size_t>(count));
        for(std::size_t i=1;i<pieces;++i)
        {
            const double time=std::lerp(start,end,static_cast<double>(i)/pieces);
            if(time<=result.back()||time>=end)throw std::invalid_argument("FBX animation sample time precision");
            result.push_back(time);
        }
        result.push_back(end);
    }
    return result;
}
FbxImportedClip bake(const ufbx_scene &scene,const ufbx_anim_stack &stack,
    std::span<const FbxImportedNode> nodes,const FbxImportedMorphs &morphs,
    std::span<const FbxImportedProgressiveControl> progressive,
    const FbxAnimationImportOptions &options,std::vector<std::string> &warnings,Budget &budget)
{
    if(!stack.anim || !std::isfinite(stack.time_begin) || !std::isfinite(stack.time_end) || stack.time_end<=stack.time_begin)
        throw std::invalid_argument("FBX animation take needs a positive finite duration");
    const double duration=stack.time_end-stack.time_begin;
    if(!std::isfinite(duration)||duration<=0)throw std::invalid_argument("FBX animation duration range");
    auto source=sourceKeys(scene,stack,nodes,morphs,progressive,options,warnings);
    std::vector<MorphGroup> groups;
    if(options.morphGrouping==FbxMorphAnimationGrouping::MergeByName)
    {
        std::map<std::string,MorphGroup,std::less<>> named;
        std::map<std::string,std::unordered_set<const ufbx_blend_channel *>,std::less<>> seen;
        for(const auto &morph:morphs)
        {
            auto &group=named[morph.name];auto &known=seen[morph.name];
            group.name=morph.name;group.targets.push_back({morph.mesh,morph.target});
            for(const auto *channel:morph.channels)
                if(known.insert(channel).second)group.channels.push_back(channel);
        }
        for(auto &entry:named)groups.push_back(std::move(entry.second));
    }
    else for(const auto &morph:morphs)
        groups.push_back({"fbx/mesh/"+std::to_string(morph.mesh)+"/target/"+std::to_string(morph.target),
                          morph.channels,{{morph.mesh,morph.target}}});
    std::vector<MorphGroup> activeMorphs;
    for(auto &group:groups)
    {
        bool active=false;for(const auto *channel:group.channels)
            active=active || source.animatedMorphs.contains(channel->element_id) || channel->weight!=0;
        if(active){group.maximumError=options.maximumMorphError;activeMorphs.push_back(std::move(group));}
    }
    std::set<std::string,std::less<>> reserved,occupied;
    for(const auto &control:progressive)
        if(!reserved.insert(object3DProgressiveControlCurveName(control.name)).second)
            throw std::invalid_argument("Duplicate FBX progressive animation control");
    occupied=reserved;for(const auto &group:activeMorphs)occupied.insert(group.name);
    std::size_t suffix=0;
    for(auto &group:activeMorphs)if(!progressive.empty()&&std::string_view(group.name).starts_with(Object3DProgressiveCurvePrefix))
    {
        std::string replacement;
        do{replacement="iiActorObject/fbx-linear/"+std::to_string(suffix++);}while(!occupied.insert(replacement).second);
        group.name=std::move(replacement);
        warn(warnings,"An ordinary morph curve was renamed to avoid the progressive-control namespace.");
    }
    for(const auto &control:progressive)
    {
        double tolerance=options.maximumMorphError;
        for(const auto *channel:control.channels)
            tolerance=std::min(tolerance,progressiveControlTolerance(*channel,options.maximumMorphError));
        activeMorphs.push_back({object3DProgressiveControlCurveName(control.name),control.channels,{},tolerance});
    }
    const auto dimensions=nodes.size()+activeMorphs.size();
    std::size_t evaluationDimensions=nodes.size();
    if(evaluationDimensions>options.maximumEvaluationValues)
        throw std::length_error("FBX animation evaluation value budget exceeded");
    for(const auto &group:activeMorphs)
    {
        if(group.channels.size()>options.maximumEvaluationValues-evaluationDimensions)
            throw std::length_error("FBX animation evaluation value budget exceeded");
        evaluationDimensions+=group.channels.size();
    }
    const auto available=options.maximumKeys-budget.keys;
    const auto limit=dimensions?available/dimensions:available;
    auto times=sampleTimes(source.times,options.maximumSampleInterval,limit);
    std::map<double,Frame> frames;
    const auto evaluate=[&](double relative)->const Frame&
    {
        if(const auto found=frames.find(relative);found!=frames.end())return found->second;
        if(budget.evaluations>=options.maximumEvaluations || evaluationDimensions>options.maximumEvaluationValues-budget.values)
            throw std::length_error("FBX animation evaluation budget exceeded");
        const double sourceTime=relative==duration?stack.time_end:stack.time_begin+relative;
        if(!std::isfinite(sourceTime) || (relative>0 && relative<duration &&
           (sourceTime<=stack.time_begin || sourceTime>=stack.time_end)))
            throw std::invalid_argument("FBX animation evaluation time loses precision");
        ufbx_evaluate_opts evaluateOptions{};evaluateOptions.temp_allocator.memory_limit=options.geometry.maxFileBytes*4;
        evaluateOptions.result_allocator.memory_limit=options.geometry.maxFileBytes*4;
        ufbx_error diagnostic{};
        std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)> evaluated(
            ufbx_evaluate_scene(&scene,stack.anim,sourceTime,&evaluateOptions,&diagnostic),ufbx_free_scene);
        if(!evaluated)throw std::invalid_argument("FBX animation evaluation: "+text(diagnostic.description));
        Frame frame;frame.bones.reserve(nodes.size());frame.morphs.reserve(activeMorphs.size());
        for(const auto &entry:nodes)
        {
            if(entry.node->typed_id>=evaluated->nodes.count || (entry.parent && entry.parent->typed_id>=evaluated->nodes.count))
                throw std::invalid_argument("FBX evaluated hierarchy changed");
            auto world=evaluated->nodes.data[entry.node->typed_id]->node_to_world;
            if(entry.parent)
            {
                const auto inverse=ufbx_matrix_invert(&evaluated->nodes.data[entry.parent->typed_id]->node_to_world);
                world=ufbx_matrix_mul(&inverse,&world);
            }
            frame.bones.push_back(transform(world,options.geometry.maximumJointMatrixError));
        }
        for(const auto &group:activeMorphs)
        {
            const auto &channels=group.channels;
            const double first=ufbx_evaluate_blend_weight(stack.anim,channels.front(),sourceTime);
            if(!std::isfinite(first))throw std::invalid_argument("Nonfinite FBX morph animation");
            for(std::size_t i=1;i<channels.size();++i)
            {
                const double value=ufbx_evaluate_blend_weight(stack.anim,channels[i],sourceTime);
                if(!std::isfinite(value)||std::abs(value-first)>group.maximumError)
                    throw std::invalid_argument("Matching FBX morph channels carry different animation values");
            }
            frame.morphs.push_back(first);
        }
        ++budget.evaluations;budget.values+=evaluationDimensions;
        return frames.emplace(relative,std::move(frame)).first->second;
    };
    for(unsigned pass=0;pass<64;++pass)
    {
        std::vector<double> extra;
        for(std::size_t i=1;i<times.size();++i)
        {
            const double left=times[i-1],right=times[i];const auto &a=evaluate(left),&b=evaluate(right);
            bool split=false;
            for(const double fraction:{.25,.5,.75})
            {
                const double at=std::lerp(left,right,fraction);
                if(at<=left||at>=right)continue;
                const double amount=(at-left)/(right-left);
                const auto &actual=evaluate(at);
                for(std::size_t j=0;j<nodes.size() && !split;++j)
                {
                    const auto predicted=character_detail::blend(a.bones[j],b.bones[j],amount);
                    split=distance(predicted.position,actual.bones[j].position)>options.maximumPositionError ||
                          distance(predicted.scale,actual.bones[j].scale)>options.maximumScaleError ||
                          rotationError(predicted.rotation,actual.bones[j].rotation)>options.maximumRotationError;
                }
                for(std::size_t j=0;j<activeMorphs.size()&&!split;++j)
                    split=std::abs(std::lerp(a.morphs[j],b.morphs[j],amount)-actual.morphs[j])>activeMorphs[j].maximumError;
                if(split)break;
            }
            if(split)
            {
                const double middle=std::midpoint(left,right);
                if(middle<=left||middle>=right)throw std::invalid_argument("FBX animation cannot be sampled at requested precision");
                extra.push_back(middle);
            }
        }
        if(extra.empty())
        {
            if(dimensions && times.size()>available/dimensions)throw std::length_error("FBX animation key budget exceeded");
            FbxImportedClip result{text(stack.name),stack.time_begin,duration,{},{},{}};
            result.tracks.reserve(nodes.size());result.curves.reserve(activeMorphs.size());
            for(const auto &entry:nodes)result.tracks.push_back({entry.bone,{}});
            for(const auto &group:activeMorphs)
            {
                const auto curve=result.curves.size();result.curves.push_back({group.name,{}});
                for(const auto &[mesh,target]:group.targets)result.morphBindings.push_back({curve,mesh,target});
            }
            for(auto &track:result.tracks)track.keys.reserve(times.size());
            for(auto &curve:result.curves)curve.keys.reserve(times.size());
            for(const auto time:times)
            {
                const auto &frame=evaluate(time);
                for(std::size_t i=0;i<result.tracks.size();++i)result.tracks[i].keys.push_back({time,frame.bones[i]});
                for(std::size_t i=0;i<result.curves.size();++i)result.curves[i].keys.push_back({time,frame.morphs[i]});
            }
            budget.keys+=times.size()*dimensions;return result;
        }
        if(extra.size()>limit-times.size())throw std::length_error("FBX animation key budget exceeded");
        times.insert(times.end(),extra.begin(),extra.end());std::sort(times.begin(),times.end());
    }
    throw std::length_error("FBX animation sampling did not converge");
}
} // namespace

std::vector<FbxImportedClip> planFbxImportedAnimations(const ufbx_scene &scene,
    std::span<const FbxImportedNode> nodes,const FbxImportedMorphs &morphs,
    std::span<const FbxImportedProgressiveControl> progressive,
    const FbxAnimationImportOptions &options,std::vector<std::string> &warnings)
{
    validateOptions(options);
    if(scene.anim_stacks.count>options.maximumClips)throw std::length_error("FBX animation take limit");
    if(scene.constraints.count && scene.anim_stacks.count)unsupported(options,warnings);
    Budget budget;std::vector<FbxImportedClip> result;result.reserve(scene.anim_stacks.count);
    for(const auto *stack:scene.anim_stacks)
    {
        if(!stack)throw std::invalid_argument("Invalid FBX animation take");
        result.push_back(bake(scene,*stack,nodes,morphs,progressive,options,warnings,budget));
    }
    return result;
}
} // namespace iiActorObject::object3d_detail
