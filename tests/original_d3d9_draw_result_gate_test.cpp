#include "../src/probe/original_d3d9_draw_result_gate.hpp"
#include "../src/probe/verified_world_source_epoch.hpp"
#include <cstdint>
#include <cstdio>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"source Draw HRESULT line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
    using namespace m2vr::vrbridge;
    constexpr std::uint32_t success=0u, positive_success=1u;
    constexpr std::uint32_t device_lost=0x88760868u, generic_failure=0x80004005u;
    for(std::uint32_t family=1u;family<=4u;++family){
        CHECK(!IsFailedOriginalD3D9Draw(family,success));
        CHECK(!IsFailedOriginalD3D9Draw(family,positive_success));
        CHECK(IsFailedOriginalD3D9Draw(family,device_lost));
        CHECK(IsFailedOriginalD3D9Draw(family,generic_failure));
        CHECK(FailedOriginalD3D9SourceKind(family)==16u+family);
    }
    CHECK(!IsFailedOriginalD3D9Draw(0u,generic_failure));
    CHECK(!IsFailedOriginalD3D9Draw(5u,generic_failure));
    CHECK(FailedOriginalD3D9SourceKind(0u)==0u);
    CHECK(FailedOriginalD3D9SourceKind(5u)==0u);
    // A failed physical original Draw revokes even an already rasterized
    // pair before Present. Retry within that exact source frame is forbidden.
    OriginalSourceFrameEpoch frames{};
    CHECK(frames.CanBegin(41u,40u));
    CHECK(frames.Revoke(41u));
    CHECK(!frames.Revoke(41u));
    CHECK(!frames.CanBegin(41u,40u));
    // Unsampled original Draw failure BEFORE the verified CPU Begin must
    // forbid the rest of that physical Present, not the next frame.
    OriginalSourceFrameEpoch prebegin{};
    CHECK(prebegin.Revoke(41u));
    CHECK(!prebegin.CanBegin(41u,40u));
    CHECK(prebegin.CanBegin(42u,40u));
    CHECK(static_cast<std::uint32_t>(OriginalSourceFailure::OriginalDrawFailed)==17u);
    CHECK(frames.CanBegin(42u,40u));
    return 0;
}
