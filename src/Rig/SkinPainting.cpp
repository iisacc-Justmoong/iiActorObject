#include "SkinPainting.h"
#include "MeshEditGraph.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace iiActorObject
{
namespace
{
using Row=std::map<BoneIndex,double>;
Row values(const std::vector<SkinWeight> &source,std::size_t bones)
{
    if(source.empty())return {{0,1}};auto normalized=source;normalizeSkinWeights(normalized,bones);Row row;for(const auto &w:normalized)if(w.weight>0)row[w.bone]=w.weight;return row;
}
double get(const Row &row,BoneIndex bone){const auto i=row.find(bone);return i==row.end()?0:i->second;}
bool same(const Row &a,const Row &b)
{
    for(const auto &[bone,v]:a)if(std::abs(v-get(b,bone))>=1e-12)return false;
    for(const auto &[bone,v]:b)if(std::abs(v-get(a,bone))>=1e-12)return false;return true;
}
Row constrained(Row desired,const Row &original,const std::set<BoneIndex> &locked,std::size_t maximum)
{
    double lockedTotal=0;for(auto b:locked){desired[b]=get(original,b);lockedTotal+=desired[b];}
    if(lockedTotal>1+1e-12)throw std::invalid_argument("Locked skin weights exceed one");const auto available=std::max(0.0,1-lockedTotal);
    std::vector<std::pair<BoneIndex,double>> unlocked;std::size_t lockedCount=0;
    for(const auto &[bone,value]:desired)
    {
        if(!std::isfinite(value)||value<0)throw std::invalid_argument("Invalid painted influence");
        if(value>0){if(locked.contains(bone))++lockedCount;else unlocked.push_back({bone,value});}
    }
    std::sort(unlocked.begin(),unlocked.end(),[](const auto &a,const auto &b){return a.second!=b.second?a.second>b.second:a.first<b.first;});
    if(maximum)
    {
        if(lockedCount>maximum||(available>1e-12&&lockedCount==maximum))throw std::length_error("Influence limit cannot retain locked weights and residual mass");
        if(unlocked.size()>maximum-lockedCount)unlocked.resize(maximum-lockedCount);
    }
    double total=0;for(const auto &[bone,value]:unlocked){(void)bone;total+=value;}
    if(available>1e-12&&!(total>0))throw std::invalid_argument("No unlocked influence can receive residual skin weight");
    Row result;for(auto b:locked)if(get(original,b)>0)result[b]=get(original,b);
    if(total>0)for(const auto &[bone,value]:unlocked)if(value>0&&available>0)result[bone]=value/total*available;return result;
}
}
BrushStrokeResult SkinPainter::paint(Object3DData &data,std::size_t mesh,BoneIndex bone,std::span<const MeshBrushSample> samples,const SkinPaintOptions &o)
{
    (void)Skeleton(data.bones);object3d_detail::MeshEditGraph graph(data.meshes.at(mesh));graph.validate(o.brush,samples.size());
    if(o.mode<SkinPaintMode::Replace||o.mode>SkinPaintMode::Smooth||!std::isfinite(o.value)||o.value<0||o.value>1||o.maximumInfluences>65535||
       (o.mode==SkinPaintMode::Smooth?bone!=NoBone:bone>=data.bones.size())||(o.fallbackBone!=NoBone&&o.fallbackBone>=data.bones.size()))throw std::invalid_argument("Invalid skin brush options");
    std::set<BoneIndex> locked;for(auto b:o.lockedBones)if(b>=data.bones.size()||!locked.insert(b).second)throw std::invalid_argument("Invalid or duplicate locked bone");
    if(locked.contains(bone)||(o.fallbackBone!=NoBone&&(locked.contains(o.fallbackBone)||o.fallbackBone==bone)))throw std::invalid_argument("Active/fallback bone conflicts with skin locks");
    const auto &source=data.meshes[mesh];std::vector<Row> rows;rows.reserve(graph.positions.size());
    for(std::size_t p=0;p<graph.positions.size();++p)rows.push_back(values(source.topology?source.topology->points[p].weights:source.geometry.vertices[p].weights,data.bones.size()));
    const auto original=rows;std::vector<bool> touched(rows.size(),false);
    for(const auto &sample:samples)
    {
        const auto footprint=graph.footprint(sample,o.brush);std::optional<std::vector<Row>> smoothingSource;
        if(o.mode==SkinPaintMode::Smooth)smoothingSource=rows;const auto &before=smoothingSource?*smoothingSource:rows;
        for(std::size_t p=0;p<rows.size();++p)
        {
            const auto alpha=footprint[p];if(alpha==0)continue;touched[p]=true;Row desired;
            if(o.mode==SkinPaintMode::Smooth)
            {
                if(graph.neighbours[p].empty())continue;Row average;
                for(auto n:graph.neighbours[p])for(const auto &[b,w]:before[n])average[b]+=w/graph.neighbours[p].size();
                for(const auto &[b,w]:before[p])desired[b]+=(1-alpha)*w;for(const auto &[b,w]:average)desired[b]+=alpha*w;
            }
            else
            {
                const auto old=get(before[p],bone);double goal=old;
                if(o.mode==SkinPaintMode::Replace)goal=std::lerp(old,o.value,alpha);
                else if(o.mode==SkinPaintMode::Add)goal=old+o.value*alpha;else goal=old-o.value*alpha;
                double lockedTotal=0,otherTotal=0;for(const auto &[b,w]:before[p]){if(locked.contains(b))lockedTotal+=w;else if(b!=bone)otherTotal+=w;}
                const auto available=std::max(0.0,1-lockedTotal);goal=std::clamp(goal,0.0,available);
                if(otherTotal==0&&available-goal>1e-12)
                {
                    if(o.fallbackBone==NoBone)goal=available;else desired[o.fallbackBone]=available-goal;
                }
                else if(otherTotal>0)for(const auto &[b,w]:before[p])if(b!=bone&&!locked.contains(b))desired[b]=w/otherTotal*(available-goal);
                desired[bone]=goal;
            }
            rows[p]=constrained(std::move(desired),before[p],locked,o.maximumInfluences);
        }
    }
    BrushStrokeResult result;auto next=data;auto &target=next.meshes[mesh];
    for(std::size_t p=0;p<rows.size();++p)
    {
        if(touched[p])++result.touchedPoints;if(same(rows[p],original[p]))continue;++result.changedPoints;
        auto &row=target.topology?target.topology->points[p].weights:target.geometry.vertices[p].weights;row.clear();for(const auto &[b,w]:rows[p])if(w>0)row.push_back({b,w});
    }
    if(result.changedPoints){object3d_detail::refreshEditedMesh(target,false);data=std::move(next);}return result;
}
} // namespace iiActorObject
