#include "../src/probe/original_d3d9_camera_com_lifetime.hpp"
#include <cassert>
#include <thread>
using m2vr::drawprobe::OriginalCameraComLifetime;
int main(){
    using Gate=OriginalCameraComLifetime;
    Gate clean;
    auto write=clean.EnterWrite();
    assert(write.valid&&write.eligible);
    clean.CompleteWrite(write,true,true);
    auto draw=clean.EnterDraw();
    assert(clean.MayForwardCpuWorld()&&draw.eligible);
    assert(clean.CompleteDraw(draw));
    auto clean_frame=clean.FinishPresent();
    assert(!clean_frame.RejectStereoSource() &&
           clean_frame.draws==1u && clean_frame.writes==1u);
    assert(!clean.MayForwardCpuWorld());
    clean.ReopenAfterPhysicalPresentReturn();
    assert(clean.MayForwardCpuWorld());
    // Slow genuine SetVSConstant/SetTransform COM straddling Draw:
    // post-COM lineage counts alone are unsafe even if returning success.
    Gate overlap;
    auto slow=overlap.EnterWrite();
    auto conflict=overlap.EnterDraw();
    assert(!overlap.MayForwardCpuWorld());
    overlap.CompleteWrite(slow,true,true);
    assert(!overlap.CompleteDraw(conflict));
    assert(overlap.FinishPresent().flags&Gate::kOverlap);
    // Shader state changes DURING an already entering source Draw.
    Gate middle;
    auto first=middle.EnterDraw();
    auto camera=middle.EnterWrite();
    middle.CompleteWrite(camera,true,true);
    assert(!middle.CompleteDraw(first));
    auto changed=middle.FinishPresent();
    assert(changed.flags&(Gate::kOverlap|Gate::kRevisionDrift));
    // Physical original Present cut is irreversible; late Set returns
    // cannot be misattributed to a new frame even with no new Draw.
    Gate crossing;
    auto pending=crossing.EnterWrite();
    assert(crossing.FinishPresent().flags&Gate::kInFlight);
    crossing.ReopenAfterPhysicalPresentReturn();
    crossing.CompleteWrite(pending,true,true);
    assert(!crossing.MayForwardCpuWorld());
    assert(crossing.FinishPresent().flags&Gate::kCrossPresent);
    crossing.ReopenAfterPhysicalPresentReturn();
    auto repair=crossing.EnterWrite();
    crossing.CompleteWrite(repair,true,true);
    auto repair_draw=crossing.EnterDraw();
    assert(crossing.CompleteDraw(repair_draw));
    assert(crossing.FinishPresent().flags==0u);
    // A source SET entering during the original Present is not a newly
    // authoritative camera even if its return lands after reopen.
    Gate sealed;
    sealed.FinishPresent();
    auto forbidden=sealed.EnterWrite();
    sealed.ReopenAfterPhysicalPresentReturn();
    sealed.CompleteWrite(forbidden,true,true);
    assert(sealed.FinishPresent().flags&Gate::kCrossPresent);
    // Reset while COM operation is in flight, or foreign/failed source.
    Gate reset;
    auto old=reset.EnterDraw();
    reset.Reset();
    assert(!reset.CompleteDraw(old));
    assert(reset.FinishPresent().flags&Gate::kCrossPresent);
    reset.ReopenAfterPhysicalPresentReturn();
    auto external=reset.EnterWrite();
    reset.CompleteWrite(external,false,true);
    assert(reset.FinishPresent().flags&Gate::kUntrustedWriter);
    reset.ReopenAfterPhysicalPresentReturn();
    auto failed=reset.EnterWrite();
    reset.CompleteWrite(failed,true,false);
    assert(reset.FinishPresent().flags&Gate::kFailedWriter);
    // Concurrent double camera writers cannot certify a single epoch.
    Gate parallel;
    auto a=parallel.EnterWrite(),b=parallel.EnterWrite();
    parallel.CompleteWrite(a,true,true);
    parallel.CompleteWrite(b,true,true);
    assert(parallel.FinishPresent().flags&Gate::kOverlap);
    return 0;
}
