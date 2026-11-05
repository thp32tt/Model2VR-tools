#include "../src/probe/original_d3d9_clear_result_gate.hpp"
#include "../src/probe/verified_world_source_epoch.hpp"
#include <cassert>
int main(){
    using namespace m2vr::vrbridge;
    static_assert(!IsFailedOriginalD3D9Clear(0u));
    static_assert(!IsFailedOriginalD3D9Clear(1u));
    static_assert(IsFailedOriginalD3D9Clear(0x8876086Cu));
    static_assert(IsFailedOriginalD3D9Clear(0x80070057u));
    static_assert(FailedOriginalD3D9ClearSourceKind()==21u);
    static_assert(static_cast<unsigned>(OriginalSourceFailure::OriginalClearFailed)==18u);
    OriginalSourceFrameEpoch source{};
    assert(source.CanBegin(61u,60u));
    assert(source.Revoke(61u));
    assert(!source.CanBegin(61u,60u));
    assert(!source.Revoke(61u));
    assert(source.CanBegin(62u,60u));
    assert(source.Revoke(62u));
    assert(!source.CanBegin(62u,60u));
    assert(source.CanBegin(63u,62u));
    return 0;
}
