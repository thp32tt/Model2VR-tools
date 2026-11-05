#include "../src/probe/original_d3d9_surface_result_gate.hpp"
#include "../src/probe/verified_world_source_epoch.hpp"
#include <cassert>
int main(){
    using namespace m2vr::vrbridge;
    for(std::uint32_t method=1u;method<=20u;++method){
        assert(IsFailedOriginalD3D9Surface(method,0x8876086Cu));
        assert(!IsFailedOriginalD3D9Surface(method,0u));
        assert(FailedOriginalD3D9SurfaceSourceKind(method)==method+21u);
    }
    static_assert(!IsFailedOriginalD3D9Surface(0u,0x80070057u));
    static_assert(!IsFailedOriginalD3D9Surface(21u,0x80070057u));
    static_assert(static_cast<unsigned>(OriginalSourceFailure::OriginalSurfaceFailed)==19u);
    OriginalSourceFrameEpoch epoch{};
    assert(epoch.CanBegin(77u,76u));
    assert(epoch.Revoke(77u));
    assert(!epoch.CanBegin(77u,76u));
    assert(epoch.CanBegin(78u,77u));
    assert(epoch.Revoke(78u));
    assert(epoch.CanBegin(79u,78u));
    return 0;
}
