#include "Object3DGlbBundleIndex.h"
#include <limits>
#include <stdexcept>
#include <utility>

namespace iiActorObject::object3d_detail::glb_import
{
GlbBundleIndex::GlbBundleIndex(std::size_t &keyBytes,std::size_t maximumKeyBytes)
    :keyBytes_(keyBytes),maximumKeyBytes_(maximumKeyBytes){}

std::optional<GlbMeshBundle> GlbBundleIndex::find(std::string_view key) const
{
    if(const auto found=bundles_.find(key);found!=bundles_.end())return found->second;
    return std::nullopt;
}
GlbBundleCandidate GlbBundleIndex::prepare(std::string key,std::string pointKey,std::size_t pointOffset) const
{
    if(bundles_.contains(key))throw std::invalid_argument("GLB import: duplicate attribute bundle");
    if(keyBytes_>maximumKeyBytes_||key.size()>maximumKeyBytes_-keyBytes_)
        throw std::invalid_argument("GLB import: attribute mapping budget exceeded");
    if(pointOffset>=std::numeric_limits<std::uint32_t>::max())throw std::length_error("GLB point bundle offset limit");
    GlbBundleCandidate candidate;candidate.owner_=this;candidate.baseBytes_=keyBytes_;candidate.revision_=revision_;
    candidate.pointOffset_=static_cast<std::uint32_t>(pointOffset);candidate.charge_=key.size();
    if(const auto found=points_.find(pointKey);found!=points_.end())candidate.shared_=found->second;
    else
    {
        if(pointKey.size()>maximumKeyBytes_-keyBytes_-candidate.charge_)
            throw std::invalid_argument("GLB import: attribute mapping budget exceeded");
        candidate.charge_+=pointKey.size();
    }
    candidate.bundleKey_=std::move(key);candidate.pointKey_=std::move(pointKey);return candidate;
}
void GlbBundleIndex::commit(GlbBundleCandidate &&candidate,GlbMeshBundle bundle)
{
    if(candidate.owner_!=this||candidate.revision_!=revision_||candidate.baseBytes_!=keyBytes_)
        throw std::invalid_argument("GLB import: stale attribute bundle candidate");
    if(revision_==std::numeric_limits<std::size_t>::max())throw std::length_error("GLB bundle index revision limit");
    const auto limit=std::numeric_limits<std::uint32_t>::max();
    if(bundle.count>limit-bundle.layoutOffset||(!candidate.shared_&&bundle.count>limit-candidate.pointOffset_))
        throw std::length_error("GLB bundle range limit");
    if(candidate.shared_&&candidate.shared_->count!=bundle.count)
        throw std::invalid_argument("GLB import: shared point bundle dimensions");
    const bool newDistinct=!candidate.shared_&&!points_.empty();
    auto insertedPoint=points_.end();
    if(!candidate.shared_)
    {
        auto [it,inserted]=points_.emplace(std::move(candidate.pointKey_),GlbPointBundle{candidate.pointOffset_,bundle.count});
        if(!inserted)throw std::invalid_argument("GLB import: duplicate point bundle");
        insertedPoint=it;
    }
    try
    {
        if(!bundles_.emplace(std::move(candidate.bundleKey_),bundle).second)
            throw std::invalid_argument("GLB import: duplicate attribute bundle");
    }
    catch(...){if(insertedPoint!=points_.end())points_.erase(insertedPoint);throw;}
    keyBytes_+=candidate.charge_;distinct_|=newDistinct;++revision_;candidate.owner_=nullptr;
}
}
