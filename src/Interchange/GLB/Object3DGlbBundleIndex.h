#pragma once
#include "Object3DGlbBundleData.h"
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace iiActorObject::object3d_detail::glb_import
{
class GlbBundleIndex;
class GlbBundleCandidate final
{
public:
    [[nodiscard]] std::optional<GlbPointBundle> sharedPoints() const noexcept{return shared_;}
private:
    friend class GlbBundleIndex;
    const GlbBundleIndex *owner_=nullptr;
    std::string bundleKey_,pointKey_;
    std::optional<GlbPointBundle> shared_;
    std::uint32_t pointOffset_=0;
    std::size_t charge_=0,baseBytes_=0,revision_=0;
};

// Tracks shared vertex/point bundles. The borrowed counter must outlive this
// index. Candidates are single-use and become stale after a successful commit.
class GlbBundleIndex final
{
public:
    explicit GlbBundleIndex(std::size_t &keyBytes,std::size_t maximumKeyBytes=16*1024*1024);
    GlbBundleIndex(const GlbBundleIndex &)=delete;
    GlbBundleIndex &operator=(const GlbBundleIndex &)=delete;
    [[nodiscard]] std::optional<GlbMeshBundle> find(std::string_view key) const;
    [[nodiscard]] GlbBundleCandidate prepare(std::string key,std::string pointKey,std::size_t pointOffset) const;
    void commit(GlbBundleCandidate &&candidate,GlbMeshBundle bundle);
    [[nodiscard]] bool distinctPointBundles() const noexcept{return distinct_;}
private:
    std::size_t &keyBytes_;
    std::size_t maximumKeyBytes_,revision_=0;
    bool distinct_=false;
    std::map<std::string,GlbMeshBundle,std::less<>> bundles_;
    std::map<std::string,GlbPointBundle,std::less<>> points_;
};
}
