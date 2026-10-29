#include "GlbAnimationPose.h"
#include "CharacterMath.h"
#include "Object3DAnimationApply.h"
#include "../third_party/ufbx/ufbx.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <unordered_map>

namespace iiActorObject
{
namespace
{
[[noreturn]] void fail(std::string_view message){throw std::invalid_argument("GLB animation pose: "+std::string(message));}
ufbx_matrix matrix(const std::array<double,16> &values)
{
    if(values[3]!=0||values[7]!=0||values[11]!=0||values[15]!=1)fail("node matrix is not affine");
    ufbx_matrix result{};
    for(unsigned c=0;c<4;++c)for(unsigned r=0;r<3;++r)
    {const auto v=values[c*4+r];if(!std::isfinite(v))fail("nonfinite node matrix");result.v[c*3+r]=v;}
    return result;
}
ufbx_matrix matrix(const Transform &transform){return matrix(transform.matrix().elements());}
ufbx_matrix inverse(const ufbx_matrix &m)
{
    const auto determinant=ufbx_matrix_determinant(&m);if(!std::isfinite(determinant)||determinant==0)fail("singular node transform");
    const auto result=ufbx_matrix_invert(&m);for(const auto v:result.v)if(!std::isfinite(v))fail("node inverse overflow");return result;
}
Transform decompose(const ufbx_matrix &m,double tolerance)
{
    (void)inverse(m);const auto t=ufbx_matrix_to_transform(&m);
    const auto result=character_detail::skeletalTransform({{t.translation.x,t.translation.y,t.translation.z},
        {t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w},{t.scale.x,t.scale.y,t.scale.z}});
    const auto roundtrip=matrix(result);double error=0;
    for(std::size_t i=0;i<12;++i)
    {const auto e=std::abs(roundtrip.v[i]-m.v[i])/std::max({1.0,std::abs(roundtrip.v[i]),std::abs(m.v[i])});if(!std::isfinite(e))fail("matrix comparison overflow");error=std::max(error,e);}
    if(error>tolerance)fail("animated joint transform contains unrepresentable shear");return result;
}
} // namespace

GlbAnimationPoseSample sampleGlbAnimationPose(const Object3DHandle &object,
    std::span<const GlbAnimationNodeBinding> nodes,const GlbAnimationSource &source,double seconds,
    AnimationWrap wrap,double maximumMatrixError)
{
    if(!std::isfinite(maximumMatrixError)||maximumMatrixError<0||maximumMatrixError>0.01)fail("invalid matrix tolerance");
    const auto &asset=object.get();const auto &skeleton=asset.skeleton();const auto bones=skeleton->bones();
    if(nodes.empty()||nodes.size()>100000)fail("invalid node count");
    std::unordered_map<std::size_t,std::size_t> index;index.reserve(nodes.size());
    std::vector<std::size_t> parents(nodes.size(),NoBone),nearest(nodes.size(),NoBone),boneNodes(bones.size(),NoBone);
    std::vector<unsigned char> hasMesh(asset.meshes().size(),0),unsafe(nodes.size(),0);
    for(std::size_t i=0;i<nodes.size();++i)
    {
        const auto &n=nodes[i];if(!index.emplace(n.sourceNode,i).second)fail("duplicate source node");
        if(n.parentSourceNode!=NoBone)
        {const auto p=index.find(n.parentSourceNode);if(p==index.end()||p->second==i)fail("node parents must precede children");parents[i]=p->second;}
        if(n.bone!=NoBone)
        {if(n.bone>=bones.size()||boneNodes[n.bone]!=NoBone)fail("invalid or duplicate bone binding");boneNodes[n.bone]=i;}
        if(n.mesh!=NoBone)
        {if(n.mesh>=hasMesh.size()||hasMesh[n.mesh])fail("invalid or duplicate mesh binding");hasMesh[n.mesh]=1;if(!n.skinnedMesh)unsafe[i]=1;}
        else if(n.skinnedMesh)fail("skinned flag without a mesh");
        nearest[i]=n.bone!=NoBone?i:(parents[i]==NoBone?NoBone:nearest[parents[i]]);
    }
    if(std::find(hasMesh.begin(),hasMesh.end(),0)!=hasMesh.end())fail("missing mesh binding");
    BoneIndex synthetic=NoBone;
    for(BoneIndex bone=0;bone<bones.size();++bone)if(boneNodes[bone]==NoBone)
    {if(synthetic!=NoBone||bones[bone].parent!=NoBone)fail("missing joint binding");synthetic=bone;}
    for(std::size_t i=0;i<nodes.size();++i)if(nodes[i].bone!=NoBone)
    {
        const auto parent=parents[i]==NoBone?NoBone:nearest[parents[i]];
        const auto expected=parent==NoBone?synthetic:nodes[parent].bone;
        if(bones[nodes[i].bone].parent!=expected)fail("bone hierarchy does not match source nodes");
    }
    for(std::size_t i=nodes.size();i-->0;)
    {
        if(nodes[i].bone!=NoBone){unsafe[i]=0;continue;}
        if(unsafe[i]&&parents[i]!=NoBone)unsafe[parents[i]]=1;
    }
    std::vector<Transform> locals;locals.reserve(nodes.size());
    for(const auto &node:nodes)locals.push_back(node.localTransform);
    GlbAnimationPoseSample output;output.object=object;output.boneLocals.reserve(bones.size());
    for(const auto &bone:bones)output.boneLocals.push_back(bone.bindTransform);
    for(const auto &value:source.sample(seconds,wrap))
    {
        const auto found=index.find(value.sourceNode);if(found==index.end())fail("animation targets a node outside the selected scene");
        const auto i=found->second;const auto &node=nodes[i];
        if(value.path==GlbAnimationPath::Weights)
        {
            if(node.mesh==NoBone)fail("animated morph has no imported mesh");
            const auto &targets=asset.data().meshes[node.mesh].geometry.morphTargets;
            if(value.values.size()!=targets.size())fail("animated morph count does not match imported mesh");
            for(std::size_t target=0;target<targets.size();++target)output.morphs.push_back({node.mesh,target,value.values[target]});
        }
        else
        {
            if(node.hasMatrix)fail("animated TRS node uses a matrix");
            if(unsafe[i])fail("animated rigid mesh transform cannot be represented by the imported skeleton");
            auto &t=locals[i];
            if(value.path==GlbAnimationPath::Translation)t.position={value.values[0],value.values[1],value.values[2]};
            else if(value.path==GlbAnimationPath::Scale)t.scale={value.values[0],value.values[1],value.values[2]};
            else if(value.path==GlbAnimationPath::Rotation)t.rotation={value.values[0],value.values[1],value.values[2],value.values[3]};
            else fail("unknown animated property");
        }
    }
    std::vector<ufbx_matrix> world;world.reserve(nodes.size());
    for(std::size_t i=0;i<nodes.size();++i)
    {
        const auto local=nodes[i].hasMatrix?matrix(nodes[i].localMatrix):matrix(locals[i]);
        world.push_back(parents[i]==NoBone?local:ufbx_matrix_mul(&world[parents[i]],&local));
        if(nodes[i].bone!=NoBone)
        {
            auto m=world[i];const auto parent=parents[i]==NoBone?NoBone:nearest[parents[i]];
            if(parent!=NoBone){const auto inv=inverse(world[parent]);m=ufbx_matrix_mul(&inv,&m);}
            output.boneLocals[nodes[i].bone]=decompose(m,maximumMatrixError);
        }
    }
    return output;
}

void applyGlbAnimationPose(const GlbAnimationPoseSample &sample,Pose &pose,
                           std::span<SkeletalMeshComponent *const> meshes)
{
    object3d_detail::applyAnimationPose(sample.object,sample.boneLocals,sample.morphs,pose,meshes,true,
                                       "GLB animation pose",sample.object.get().hasRig());
}
} // namespace iiActorObject
