#include "../src/probe/verified_world_source_epoch.hpp"
#include <cstdint>
#include <cstdio>
#define CHECK(x) do{ if(!(x)){std::fprintf(stderr,"world source epoch line %d\n",__LINE__);return 1;} }while(0)
int main() {
    using m2vr::vrbridge::OriginalSourceDrawKind;
    CHECK(static_cast<std::uint32_t>(OriginalSourceDrawKind::NativeCpuRaster)==0u);
    CHECK(static_cast<std::uint32_t>(OriginalSourceDrawKind::ProjectedIndexedVertexBuffer)==10u);
    CHECK(static_cast<std::uint32_t>(OriginalSourceDrawKind::ProjectedTopologyIndexedUP)==16u);
    m2vr::vrbridge::OriginalSourceFrameEpoch epoch;
    CHECK(!epoch.CanBegin(0u,0u));
    CHECK(epoch.CanBegin(13u,12u));
    CHECK(!epoch.Revoke(0u));
    CHECK(epoch.Revoke(13u));
    CHECK(epoch.LastRejected()==13u);
    CHECK(!epoch.Revoke(13u));
    CHECK(!epoch.Revoke(12u));
    CHECK(!epoch.CanBegin(13u,12u)); // no partial source same-frame retry
    CHECK(!epoch.CanBegin(12u,0u));
    CHECK(epoch.CanBegin(14u,12u)); // next physical Present may recover
    CHECK(!epoch.CanBegin(14u,14u)); // successful source frame is monotonic
    CHECK(epoch.Revoke(15u));
    CHECK(!epoch.CanBegin(14u,0u)); // older rejected frames stay rejected
    CHECK(epoch.CanBegin(16u,15u));
    return 0;
}
