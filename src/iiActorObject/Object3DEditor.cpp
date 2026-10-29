#include "Object3DEditor.h"
#include "CharacterMath.h"
#include "MeshGeometry.h"
#include "Object3DAdjustment.h"
#include "Object3DSelectionState.h"
#include <map>
#include <unordered_set>

namespace iiActorObject
{
using namespace character_detail;
namespace
{
auto adjacency(const SkeletalMeshLod &mesh)
{
    std::vector<std::vector<std::size_t>> neighbors(mesh.vertices.size());
    for (std::size_t i = 0; i < mesh.triangles.size(); i += 3)
        for (unsigned j = 0; j < 3; ++j)
        {
            const auto a = mesh.triangles[i + j], b = mesh.triangles[i + (j + 1) % 3];
            neighbors[a].push_back(b);
            neighbors[b].push_back(a);
        }
    for (auto &v : neighbors)
    {
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
    }
    return neighbors;
}
void smoothing(double strength, unsigned iterations)
{
    weight(strength);
    if (iterations == 0 || iterations > 100)
        throw std::invalid_argument("Smoothing iterations must be in [1,100]");
}
void reconcileEdit(Object3DData &data,std::size_t mesh)
{
    auto &part=data.meshes.at(mesh);
    if (part.topology)
        reconcileTopologyRender(*part.topology,part.geometry,part.textureCoordinates,data.bones.size());
}
bool sameSkin(const Object3DMesh &a,const Object3DMesh &b)
{
    if(a.topology.has_value()!=b.topology.has_value())return false;
    const auto &left=a.topology?a.topology->skinning:a.geometry.skinning;
    const auto &right=b.topology?b.topology->skinning:b.geometry.skinning;
    if(left!=right)return false;
    const auto sameRows=[](const auto &first,const auto &second)
    {
        if(first.size()!=second.size())return false;
        for(std::size_t i=0;i<first.size();++i)
        {
            const auto &x=first[i].weights,&y=second[i].weights;
            if(x.size()!=y.size())return false;
            for(std::size_t j=0;j<x.size();++j)
                if(x[j].bone!=y[j].bone||x[j].weight!=y[j].weight)return false;
        }
        return true;
    };
    return a.topology?sameRows(a.topology->points,b.topology->points):sameRows(a.geometry.vertices,b.geometry.vertices);
}
MeshTopology &authored(Object3DData &data,std::size_t mesh)
{
    auto &part=data.meshes.at(mesh);
    if (!part.topology) throw std::invalid_argument("This operation requires source authoring topology");
    return *part.topology;
}
template<class Point> void smoothPositions(std::vector<Point> &points,
    const std::vector<std::vector<std::size_t>> &neighbors,double strength,unsigned iterations)
{
    for (unsigned step=0;step<iterations;++step)
    {
        auto next=points;
        for (std::size_t v=0;v<points.size();++v)
        {
            if (neighbors[v].empty()) continue;
            Vector3 sum;
            for (const auto n:neighbors[v]) sum=add(sum,points[n].position);
            next[v].position=add(mul(points[v].position,1-strength),mul(sum,strength/neighbors[v].size()));
        }
        points.swap(next);
    }
}
template<class Point> void smoothPointWeights(std::vector<Point> &points,
    const std::vector<std::vector<std::size_t>> &neighbors,double strength,unsigned iterations,std::size_t maxInfluences)
{
    for (unsigned step=0;step<iterations;++step)
    {
        auto next=points;
        for (std::size_t v=0;v<points.size();++v)
        {
            if (neighbors[v].empty()) continue;
            std::map<BoneIndex,double> sums;
            for (const auto w:points[v].weights) sums[w.bone]+=(1-strength)*w.weight;
            for (const auto n:neighbors[v])
                for (const auto w:points[n].weights) sums[w.bone]+=strength*w.weight/neighbors[v].size();
            auto &weights=next[v].weights;weights.clear();
            for (const auto [bone,value]:sums)
                if (value>0) weights.push_back({bone,value});
            std::sort(weights.begin(),weights.end(),[](auto a,auto b)
                { return a.weight!=b.weight?a.weight>b.weight:a.bone<b.bone; });
            if (weights.size()>maxInfluences) weights.resize(maxInfluences);
            double total=0;for (const auto w:weights) total+=w.weight;
            for (auto &w:weights) w.weight/=total;
        }
        points.swap(next);
    }
}
} // namespace
Object3DEditor::Object3DEditor(Object3DReference owner, Object3DHandle object, std::size_t historyLimit)
    : owner_(std::move(owner)), limit_(historyLimit)
{
    if (!owner_.owns(object) || limit_ == 0 || limit_ > 4096)
        throw std::invalid_argument("Invalid editor object, owner or history limit");
    history_.push_back({std::move(object), "Initial",{}, {},{}});
}
const Object3DHandle &Object3DEditor::object() const noexcept
{
    return history_[cursor_].object;
}
std::uint64_t Object3DEditor::revision() const noexcept
{
    return revision_;
}
std::string_view Object3DEditor::editLabel() const noexcept
{
    return history_[cursor_].label;
}
void Object3DEditor::checkRevision() const
{
    if (revision_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Editor revision exhausted");
}
void Object3DEditor::commit(Object3DData data, std::string label)
{
    if(!layered()) { publish(std::move(data),std::move(label),{});return; }
    auto state=copyLayers();
    const bool referenceChanged=data.referencePose!=object().get().data().referencePose;
    state->referencePose=data.referencePose;
    auto adjustment=std::make_shared<object3d_detail::Adjustment>(
        object3d_detail::difference(object().get().data(),data));
    const bool geometryChanged=!adjustment->empty();if(!geometryChanged&&!referenceChanged)return;
    if(geometryChanged)
    {
        if(state->layers.size()>=4096 || nextLayer_==std::numeric_limits<Object3DLayerId>::max())
            throw std::length_error("3D edit layer limit reached");
        const auto normalKeys=object3d_detail::normalAdjustmentKeys(*adjustment);
        if(!normalKeys.empty())object3d_detail::clearNormalAdjustments(*adjustment);
        state->layers.push_back({{nextLayer_,label,true,1,object3d_detail::layerEvaluationFor(state->program.get().definition())},adjustment});
        if(!normalKeys.empty())
        {
            const auto predicted=rebuild(*state);
            object3d_detail::setNormalAdjustments(*adjustment,predicted,data,normalKeys);
        }
    }
    auto result=rebuild(*state);publish(std::move(result),std::move(label),std::move(state));if(geometryChanged)++nextLayer_;
}
void Object3DEditor::publish(Object3DData data,std::string label,
    std::shared_ptr<const object3d_detail::LayerState> layers)
{
    checkRevision();
    const auto selections=history_[cursor_].selections;
    if(selections)object3d_detail::validatePointSelectionTransition(object().get().data(),data,*selections);
    std::vector<Entry> history(history_.begin(),
                               history_.begin() + static_cast<std::ptrdiff_t>(cursor_ + 1));
    history.reserve(history.size() + 1);
    auto next = owner_.createObject(std::move(data));
    history.push_back({std::move(next), std::move(label),std::move(layers),selections,{}});
    if (history.size() > limit_ + 1)
        history.erase(history.begin());
    history_.swap(history);
    cursor_ = history_.size() - 1;
    ++revision_;
    history.clear();
    owner_.collectGarbage();
}
void Object3DEditor::replace(Object3DData data, std::string label)
{
    commit(std::move(data), std::move(label));
}
void Object3DEditor::setReferencePose(std::optional<PoseReference> reference)
{
    if(reference==object().get().data().referencePose)return;
    auto data=object().get().data();data.referencePose=std::move(reference);commit(std::move(data),"Set reference pose");
}
bool Object3DEditor::setMaterialSurface(std::size_t material,Object3DMaterialSurface surface)
{
    auto data=object().get().data();auto &target=data.materials.at(material);
    if(target.surface==surface)return false;
    target.surface=surface;commit(std::move(data),"Set material surface");return true;
}
void Object3DEditor::setSkinningProfile(std::size_t mesh,SkinningProfile profile)
{
    auto data=object().get().data();auto &part=data.meshes.at(mesh);
    const auto count=part.topology?part.topology->points.size():part.geometry.vertices.size();
    validateSkinningProfile(profile,count);
    auto &current=part.topology?part.topology->skinning:part.geometry.skinning;
    if(current==profile)return;
    current=std::move(profile);commit(std::move(data),"Set skinning profile");
}
void Object3DEditor::moveVertices(std::size_t mesh, std::span<const VertexMove> moves)
{
    auto d = object().get().data();
    auto &g = d.meshes.at(mesh).geometry;
    std::unordered_set<std::size_t> seen;
    for (const auto &m : moves)
    {
        if (!finite(m.displacement) || !seen.insert(m.vertex).second)
            throw std::invalid_argument("Invalid or duplicate vertex move");
        auto &p = g.vertices.at(m.vertex).position;
        p = add(p, m.displacement);
    }
    if (moves.empty())
        return;
    recomputeNormals(g);
    reconcileEdit(d,mesh);
    commit(std::move(d), "Move vertices");
}
void Object3DEditor::moveControlPoints(std::size_t mesh,std::span<const TopologyPointMove> moves)
{
    auto d=object().get().data();auto &t=authored(d,mesh);
    std::unordered_set<std::uint32_t> seen;
    for (const auto &move:moves)
    {
        if (!finite(move.displacement)||!seen.insert(move.point).second)
            throw std::invalid_argument("Invalid or duplicate control point move");
        auto &point=t.points.at(move.point);point.position=add(point.position,move.displacement);
    }
    if (moves.empty()) return;
    recomputeTopologyNormals(t);commit(std::move(d),"Move source control points");
}
void Object3DEditor::setControlWeights(std::size_t mesh,std::span<const TopologyPointWeights> edits)
{
    auto d=object().get().data();auto &t=authored(d,mesh);
    std::unordered_set<std::uint32_t> seen;
    for (const auto &edit:edits)
    {
        if (!seen.insert(edit.point).second) throw std::invalid_argument("Duplicate control weight edit");
        t.points.at(edit.point).weights=edit.weights;
    }
    if (!edits.empty()) commit(std::move(d),"Set source skin weights");
}
void Object3DEditor::setControlMorph(std::size_t mesh,TopologyMorph morph)
{
    auto d=object().get().data();auto &t=authored(d,mesh);
    auto found=t.morphTargets.end();
    for (auto i=t.morphTargets.begin();i!=t.morphTargets.end();++i)
    {
        const bool match=morph.sourceTarget!=NoTopologyIndex?i->sourceTarget==morph.sourceTarget:i->name==morph.name;
        if (match)
        {
            if (found!=t.morphTargets.end()) throw std::invalid_argument("Ambiguous source morph; specify its source target ID");
            found=i;
        }
    }
    if (found==t.morphTargets.end()) t.morphTargets.push_back(std::move(morph));
    else
    {
        if(found->name!=morph.name)
        {
            auto &defaults=d.meshes.at(mesh).morphDefaults;
            const auto valueFor=[&](std::string_view name)
            {const auto i=std::find_if(defaults.begin(),defaults.end(),[&](const auto &v){return v.name==name;});
             return i==defaults.end()?0.:i->value;};
            const auto oldName=found->name;const auto oldValue=valueFor(oldName);
            const bool oldRemains=std::any_of(t.morphTargets.begin(),t.morphTargets.end(),
                [&](const auto &v){return &v!=&*found&&v.name==oldName;});
            const bool newExists=std::any_of(t.morphTargets.begin(),t.morphTargets.end(),
                [&](const auto &v){return &v!=&*found&&v.name==morph.name;});
            if(newExists&&valueFor(morph.name)!=oldValue)
                throw std::invalid_argument("Renaming would merge morphs with different default weights");
            if(!oldRemains)std::erase_if(defaults,[&](const auto &v){return v.name==oldName;});
            if(!newExists&&oldValue!=0)defaults.push_back({morph.name,oldValue});
        }
        if (morph.sourceTarget==NoTopologyIndex) morph.sourceTarget=found->sourceTarget;
        *found=std::move(morph);
    }
    commit(std::move(d),"Set source morph target");
}
SurfaceBindResult Object3DEditor::bindSurfaceWeights(std::size_t mesh,std::span<const BoneEnvelope> envelopes,const SurfaceBindOptions &options)
{
    auto data=object().get().data();auto result=SkinBinder::bindSurfaceMesh(data,mesh,envelopes,options);
    if(result.boundPoints)commit(std::move(data),"Bind surface skin weights");return result;
}
SkeletonSurfaceBindResult Object3DEditor::bindSurfaceWeightsFromSkeleton(std::size_t mesh,
    const SkeletonEnvelopeOptions &inference,const SurfaceBindOptions &binding)
{
    const auto &skeleton=object().get().skeleton();
    if(!skeleton)throw std::invalid_argument("Automatic surface binding requires a skeleton");
    auto inferred=SkinBinder::inferEnvelopes(*skeleton,inference);
    auto result=bindSurfaceWeights(mesh,inferred.envelopes,binding);
    return {std::move(inferred),result};
}
SkinTransferResult Object3DEditor::transferWeights(std::size_t mesh,const Object3DAsset &source,std::size_t sourceMesh,const SkinTransferOptions &options)
{return transferWeights(mesh,source,sourceMesh,options,SkinningProfileTransfer::PreserveTarget);}
SkinTransferResult Object3DEditor::transferWeights(std::size_t mesh,const Object3DAsset &source,std::size_t sourceMesh,const SkinTransferOptions &options,SkinningProfileTransfer profile)
{
    auto data=object().get().data();auto result=SkinBinder::transfer(source.data(),sourceMesh,data,mesh,options,profile);
    if(result.transferred)commit(std::move(data),"Transfer surface skin weights");return result;
}
SkinNormalTransferResult Object3DEditor::transferWeightsWithNormalTies(std::size_t mesh,const Object3DAsset &source,
    std::size_t sourceMesh,const SkinTransferOptions &options,SkinningProfileTransfer profile,
    const SkinNormalTransferOptions &normals,SurfaceQueryBudget &budget)
{
    const auto &current=object().get().data();auto data=current;
    auto result=SkinBinder::transferWithNormalTies(source.data(),sourceMesh,data,mesh,options,profile,normals,budget);
    if(result.transfer.transferred&&!sameSkin(current.meshes.at(mesh),data.meshes.at(mesh)))
        commit(std::move(data),"Transfer surface skin weights with normal ties");
    return result;
}
MeshSymmetryResult Object3DEditor::symmetrizeMesh(std::size_t mesh,const MeshSymmetryMap &mapping,const MeshSymmetryEditOptions &options)
{
    auto data=object().get().data();auto result=iiActorObject::symmetrizeMesh(data.meshes.at(mesh),mapping,options);
    if(result.changedPoints)commit(std::move(data),"Symmetrize mesh positions");return result;
}
MorphMirrorResult Object3DEditor::mirrorMorphs(std::size_t mesh,const MeshSymmetryMap &mapping,std::span<const MorphMirrorRequest> requests,const MorphMirrorOptions &options)
{
    auto data=object().get().data();auto result=iiActorObject::mirrorMorphs(data.meshes.at(mesh),mapping,requests,options);
    if(result.changed())commit(std::move(data),"Mirror morph targets");return result;
}
SurfaceFitResult Object3DEditor::fitSurface(std::size_t mesh,std::span<const SurfaceCollider *const> obstacles,const SurfaceFitOptions &options)
{
    auto data=object().get().data();auto result=fitMeshSurface(data.meshes.at(mesh),obstacles,options);
    if(result.changedPoints)commit(std::move(data),"Fit surface clearance");return result;
}
SurfaceFitResult Object3DEditor::fitSurface(std::size_t mesh,const SurfaceCollider &obstacle,const SurfaceFitOptions &options)
{const std::array<const SurfaceCollider*,1> obstacles{&obstacle};return fitSurface(mesh,obstacles,options);}
SkinTransferResult Object3DEditor::mirrorWeights(std::size_t mesh,std::span<const BoneIndex> boneMirrors,const SkinMirrorOptions &options)
{return mirrorWeights(mesh,boneMirrors,options,SkinningProfileTransfer::PreserveTarget);}
SkinTransferResult Object3DEditor::mirrorWeights(std::size_t mesh,std::span<const BoneIndex> boneMirrors,const SkinMirrorOptions &options,SkinningProfileTransfer profile)
{
    auto data=object().get().data();auto result=SkinBinder::mirror(data,mesh,boneMirrors,options,profile);
    if(result.transferred)commit(std::move(data),"Mirror surface skin weights");return result;
}
void Object3DEditor::setTopologyTextureCoordinates(std::size_t mesh,std::span<const TopologyUvEdit> edits)
{
    if(!object().get().data().meshes.at(mesh).topology)throw std::logic_error("Source UV editing requires authoring topology");
    setTextureCoordinates(mesh,0,edits);
}
void Object3DEditor::setTextureCoordinates(std::size_t mesh,std::size_t set,std::span<const TopologyUvEdit> edits)
{
    auto d=object().get().data();auto &part=d.meshes.at(mesh);
    auto &uv=part.topology?(set==0?part.topology->textureCoordinates:part.topology->additionalUvSets.at(set-1).coordinates):
                            (set==0?part.textureCoordinates:part.additionalUvSets.at(set-1).coordinates);
    std::unordered_set<std::uint32_t> seen;
    bool changed=false;
    for (const auto &edit:edits)
    {
        if (!seen.insert(edit.index).second || !std::isfinite(edit.value.u) || !std::isfinite(edit.value.v))
            throw std::invalid_argument("Invalid or duplicate UV edit");
        auto &value=uv.at(edit.index);changed|=value!=edit.value;value=edit.value;
    }
    if(changed)commit(std::move(d),"Set UV coordinates");
}
void Object3DEditor::sculpt(std::size_t mesh, Vector3 center, double radius, Vector3 displacement,
                            bool symmetry)
{
    if (!finite(center) || !finite(displacement) || !std::isfinite(radius) || radius <= 0)
        throw std::invalid_argument("Invalid sculpt brush");
    const auto &g = object().get().data().meshes.at(mesh).geometry;
    std::vector<VertexMove> moves;
    const Vector3 mirrored{-center.x, center.y, center.z};
    for (std::size_t i = 0; i < g.vertices.size(); ++i)
    {
        const auto p = g.vertices[i].position;
        const auto a = length(sub(p, center)), b = symmetry ? length(sub(p, mirrored)) : a;
        const auto distance = std::min(a, b);
        if (distance >= radius)
            continue;
        const double t = 1 - distance / radius, falloff = t * t * (3 - 2 * t);
        auto delta = displacement;
        if (b < a)
            delta.x = -delta.x;
        moves.push_back({i, mul(delta, falloff)});
    }
    moveVertices(mesh, moves);
}
BrushStrokeResult Object3DEditor::sculptStroke(std::size_t mesh,std::span<const SculptStrokeSample> samples,const MeshBrushSettings &settings,std::optional<BrushMirrorPlane> symmetry)
{
    auto data=object().get().data();auto result=MeshSculptor::stroke(data,mesh,samples,settings,symmetry);if(result.changedPoints)commit(std::move(data),"Sculpt brush stroke");return result;
}
BrushStrokeResult Object3DEditor::paintWeights(std::size_t mesh,BoneIndex bone,std::span<const MeshBrushSample> samples,const SkinPaintOptions &options)
{
    auto data=object().get().data();auto result=SkinPainter::paint(data,mesh,bone,samples,options);if(result.changedPoints)commit(std::move(data),"Paint skin weight stroke");return result;
}
void Object3DEditor::smooth(std::size_t mesh,double strength,unsigned iterations)
{
    smoothing(strength,iterations);
    auto d=object().get().data();auto &part=d.meshes.at(mesh);
    if (strength==0) return;
    if (part.topology)
    {
        auto &t=*part.topology;
        smoothPositions(t.points,topologyAdjacency(*t.layout),strength,iterations);
        recomputeTopologyNormals(t);
    }
    else
    {
        smoothPositions(part.geometry.vertices,adjacency(part.geometry),strength,iterations);
        recomputeNormals(part.geometry);
    }
    commit(std::move(d),"Smooth vertices");
}

void Object3DEditor::setMorph(std::size_t mesh, MorphTarget morph)
{
    auto d = object().get().data();
    auto &g = d.meshes.at(mesh).geometry;
    const auto found = std::find_if(g.morphTargets.begin(), g.morphTargets.end(),
                                    [&](const auto &m) { return m.name == morph.name; });
    if (found == g.morphTargets.end())
        g.morphTargets.push_back(std::move(morph));
    else
        *found = std::move(morph);
    recomputeNormals(g);
    reconcileEdit(d,mesh);
    commit(std::move(d), "Set morph");
}
void Object3DEditor::setDefaultMorphWeight(std::size_t mesh,std::string_view name,double value)
{
    if(!std::isfinite(value))throw std::invalid_argument("Morph weight must be finite");
    auto data=object().get().data();auto &part=data.meshes.at(mesh);
    if(std::none_of(part.geometry.morphTargets.begin(),part.geometry.morphTargets.end(),
                    [&](const auto &target){return target.name==name;}))
        throw std::invalid_argument("Unknown morph target");
    auto found=std::find_if(part.morphDefaults.begin(),part.morphDefaults.end(),
                            [&](const auto &entry){return entry.name==name;});
    if(found==part.morphDefaults.end())
    {
        if(value==0)return;
        part.morphDefaults.push_back({std::string(name),value});
    }
    else
    {
        if(found->value==value)return;
        if(value==0)part.morphDefaults.erase(found);else found->value=value;
    }
    commit(std::move(data),"Set default morph weight");
}
void Object3DEditor::createMorph(std::size_t mesh, std::string name, std::span<const Vector3> target)
{
    const auto &g = object().get().data().meshes.at(mesh).geometry;
    if (target.size() != g.vertices.size())
        throw std::invalid_argument("Morph topology must match");
    MorphTarget morph{std::move(name), {}};
    for (std::size_t i = 0; i < target.size(); ++i)
    {
        if (!finite(target[i]))
            throw std::invalid_argument("Nonfinite morph target");
        const auto delta = sub(target[i], g.vertices[i].position);
        if (length(delta) != 0)
            morph.deltas.push_back({i, delta, {}});
    }
    setMorph(mesh, std::move(morph));
}
void Object3DEditor::bakeMorphs(std::size_t mesh, std::span<const MorphValue> values)
{
    auto d = object().get().data();
    auto &g = d.meshes.at(mesh).geometry;
    std::unordered_set<std::string> seen;
    for (const auto &v : values)
    {
        if (!std::isfinite(v.value) || !seen.insert(v.name).second)
            throw std::invalid_argument("Invalid or duplicate morph value");
        const auto m = std::find_if(g.morphTargets.begin(), g.morphTargets.end(),
                                    [&](const auto &s) { return s.name == v.name; });
        if (m == g.morphTargets.end())
            throw std::invalid_argument("Unknown morph target");
        for (const auto &delta : m->deltas)
            g.vertices[delta.vertex].position =
                add(g.vertices[delta.vertex].position, mul(delta.position, v.value));
    }
    if (values.empty())
        return;
    recomputeNormals(g);
    reconcileEdit(d,mesh);
    commit(std::move(d), "Bake morphs");
}
void Object3DEditor::setWeights(std::size_t mesh, std::span<const VertexWeights> edits)
{
    auto d = object().get().data();
    auto &g = d.meshes.at(mesh).geometry;
    std::unordered_set<std::size_t> seen;
    for (const auto &e : edits)
    {
        if (!seen.insert(e.vertex).second)
            throw std::invalid_argument("Duplicate weight edit");
        g.vertices.at(e.vertex).weights = e.weights;
    }
    if (!edits.empty())
    {
        reconcileEdit(d,mesh);
        commit(std::move(d), "Set skin weights");
    }
}
void Object3DEditor::smoothWeights(std::size_t mesh,double strength,unsigned iterations,std::size_t maxInfluences)
{
    smoothing(strength,iterations);
    if (maxInfluences==0||maxInfluences>32) throw std::invalid_argument("Influence count must be in [1,32]");
    auto d=object().get().data();auto &part=d.meshes.at(mesh);
    if (strength==0) return;
    if (part.topology)
        smoothPointWeights(part.topology->points,topologyAdjacency(*part.topology->layout),strength,iterations,maxInfluences);
    else smoothPointWeights(part.geometry.vertices,adjacency(part.geometry),strength,iterations,maxInfluences);
    commit(std::move(d),"Smooth skin weights");
}

void Object3DEditor::moveJoint(BoneIndex bone, Vector3 position, bool preserveChildren)
{
    if (!finite(position))
        throw std::invalid_argument("Nonfinite joint position");
    auto d = object().get().data();
    auto &joint = d.bones.at(bone);
    Pose pose(object().get().skeleton());
    const auto world = pose.worldMatrices();
    const auto parentWorld = joint.parent == NoBone ? Matrix4{} : world[joint.parent];
    joint.bindTransform.position = parentWorld.invertedAffine().transformPoint(position);
    const auto newWorld = parentWorld * joint.bindTransform.matrix();
    if (preserveChildren)
        for (std::size_t i = 0; i < d.bones.size(); ++i)
            if (d.bones[i].parent == bone)
                d.bones[i].bindTransform.position =
                    newWorld.invertedAffine().transformPoint(world[i].transformPoint({}));
    commit(std::move(d), "Move rest joint");
}
bool Object3DEditor::setJointTransform(BoneIndex bone,Transform value,JointTransformSpace space,bool preserveChildren)
{
    auto data=object().get().data();if(!SkeletonEditor::setTransform(data,bone,value,space,preserveChildren))return false;
    const auto previous=revision_;commit(std::move(data),"Edit rest joint transform");return revision_!=previous;
}
bool Object3DEditor::reparentJoint(BoneIndex bone,BoneIndex parent,bool preserveWorld)
{
    auto data=object().get().data();if(!SkeletonEditor::reparent(data,bone,parent,preserveWorld))return false;
    const auto previous=revision_;commit(std::move(data),"Reparent rest joint");return revision_!=previous;
}
bool Object3DEditor::orientJoint(BoneIndex bone,Vector3 target,Vector3 up,bool preserveChildren)
{
    auto data=object().get().data();if(!SkeletonEditor::orient(data,bone,target,up,preserveChildren))return false;
    const auto previous=revision_;commit(std::move(data),"Orient rest joint");return revision_!=previous;
}
void Object3DEditor::renameJoint(BoneIndex bone, std::string name)
{
    auto d = object().get().data();
    d.bones.at(bone).name = std::move(name);
    commit(std::move(d), "Rename joint");
}
bool Object3DEditor::renameJoints(std::span<const JointRename> names)
{
    const auto &asset=object().get();
    if(names.empty())return false;
    if(names.size()>asset.data().bones.size())throw std::length_error("Joint rename count exceeds skeleton size");
    auto data=asset.data();if(!SkeletonEditor::rename(data,names))return false;
    const auto previous=revision_;commit(std::move(data),"Rename joints");return revision_!=previous;
}
bool Object3DEditor::undo()
{
    if (cursor_ == 0)
        return false;
    checkRevision();
    if (!history_[cursor_ - 1].object.valid())
        throw std::logic_error("Undo target was removed by the 3D engine");
    --cursor_;
    ++revision_;
    return true;
}
bool Object3DEditor::redo()
{
    if (cursor_ + 1 == history_.size())
        return false;
    checkRevision();
    if (!history_[cursor_ + 1].object.valid())
        throw std::logic_error("Redo target was removed by the 3D engine");
    ++cursor_;
    ++revision_;
    return true;
}
} // namespace iiActorObject
