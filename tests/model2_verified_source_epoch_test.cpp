#include "../src/probe/verified_world_source_epoch.hpp"
#include <cstdint>
#include <cstdio>
#define CHECK(x) do{ if(!(x)){std::fprintf(stderr,"world source epoch line %d\n",__LINE__);return 1;} }while(0)
int main() {
    using m2vr::vrbridge::OriginalSourceDrawKind;
    CHECK(static_cast<std::uint32_t>(OriginalSourceDrawKind::NativeCpuRaster)==0u);
    CHECK(static_cast<std::uint32_t>(OriginalSourceDrawKind::ProjectedIndexedVertexBuffer)==10u);
    CHECK(static_cast<std::uint32_t>(OriginalSourceDrawKind::ProjectedTopologyIndexedUP)==16u);
    using m2vr::vrbridge::ScopedOriginalSourceDrawKind;
    using m2vr::vrbridge::CurrentOriginalSourceDrawKind;
    CHECK(CurrentOriginalSourceDrawKind()==0u);
    {
        ScopedOriginalSourceDrawKind a(OriginalSourceDrawKind::TopologyIndexedUP);
        CHECK(CurrentOriginalSourceDrawKind()==15u);
        {
            ScopedOriginalSourceDrawKind b(OriginalSourceDrawKind::PrimitiveUP);
            CHECK(CurrentOriginalSourceDrawKind()==15u); // outer original source
        }
        CHECK(CurrentOriginalSourceDrawKind()==15u);
    }
    CHECK(CurrentOriginalSourceDrawKind()==0u);
    {
        ScopedOriginalSourceDrawKind a(OriginalSourceDrawKind::VertexBuffer);
        CHECK(CurrentOriginalSourceDrawKind()==7u);
    }
    CHECK(CurrentOriginalSourceDrawKind()==0u);
    using m2vr::vrbridge::OriginalSourceFailure;
    using m2vr::vrbridge::CurrentOriginalSourceFailure;
    using m2vr::vrbridge::RejectOriginalSourceMaterial;
    CHECK(CurrentOriginalSourceFailure()==0u);
    {
        ScopedOriginalSourceDrawKind draw(OriginalSourceDrawKind::IndexedVertexBuffer);
        CHECK(!RejectOriginalSourceMaterial(OriginalSourceFailure::Sampler));
        CHECK(CurrentOriginalSourceFailure()==8u);
        {
            ScopedOriginalSourceDrawKind inner(OriginalSourceDrawKind::PrimitiveUP);
            CHECK(CurrentOriginalSourceDrawKind()==8u);
            CHECK(CurrentOriginalSourceFailure()==8u);
            CHECK(!RejectOriginalSourceMaterial(OriginalSourceFailure::Shader));
            CHECK(CurrentOriginalSourceFailure()==8u); // first cause wins
        }
        CHECK(CurrentOriginalSourceFailure()==8u);
    }
    CHECK(CurrentOriginalSourceFailure()==0u); // no leak across draws
    CHECK(static_cast<std::uint32_t>(OriginalSourceFailure::ProjectionPass)==10u);
    {
        ScopedOriginalSourceDrawKind projected(OriginalSourceDrawKind::ProjectedPrimitiveUP);
        CHECK(!RejectOriginalSourceMaterial(OriginalSourceFailure::ProjectionPass));
        CHECK(CurrentOriginalSourceFailure()==10u);
        CHECK(!RejectOriginalSourceMaterial(OriginalSourceFailure::Sampler));
        CHECK(CurrentOriginalSourceFailure()==10u); // first stage wins
    }
    CHECK(CurrentOriginalSourceFailure()==0u);
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
