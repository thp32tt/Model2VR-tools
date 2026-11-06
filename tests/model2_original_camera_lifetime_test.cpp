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
           clean_frame.draws==1u && clean_frame.writes==1u &&
           clean_frame.camera_writes==1u && clean_frame.pixel_writes==0u &&
           clean_frame.layout_writes==0u);
    // Three genuine original source-side COM families retain independent
    // pointer-free coverage and cannot be mislabelled camera matrix writes.
    Gate families;
    auto f1=families.EnterWrite(1u);
    families.CompleteWrite(f1,true,true);
    auto f2=families.EnterWrite(2u);
    families.CompleteWrite(f2,true,true);
    auto f3=families.EnterWrite(3u);
    families.CompleteWrite(f3,true,true);
    const auto result=families.FinishPresent();
    assert(result.flags==0u && result.writes==3u &&
           result.camera_writes==1u && result.pixel_writes==1u &&
           result.layout_writes==1u);
    Gate undefined;
    auto bad_family=undefined.EnterWrite(4u);
    undefined.CompleteWrite(bad_family,true,true);
    assert(undefined.FinishPresent().flags&Gate::kUntrustedWriter);
    assert(!clean.MayForwardCpuWorld());
    clean.ReopenAfterPhysicalPresentReturn();
    assert(clean.MayForwardCpuWorld());
    // Read-only original GetTransform/GetFVF/GetStreamSource grouped under
    // the SAME real Draw preflight, never independently promoted to stereo.
    Gate read_clean;
    const auto clean_draw=read_clean.EnterDraw();
    const auto clean_read=read_clean.EnterSourceRead();
    assert(clean_read.valid && clean_read.eligible);
    assert(!read_clean.MayForwardCpuWorld()); // preflight not yet retained
    assert(read_clean.CompleteSourceRead(clean_read));
    assert(read_clean.MayForwardCpuWorld());
    assert(read_clean.CompleteDraw(clean_draw));
    assert(read_clean.FinishPresent().flags==0u);
    Gate read_overlap;
    const auto overlapping_draw=read_overlap.EnterDraw();
    const auto partial_camera=read_overlap.EnterSourceRead();
    const auto setter=read_overlap.EnterWrite();
    read_overlap.CompleteWrite(setter,true,true);
    assert(!read_overlap.CompleteSourceRead(partial_camera));
    assert(!read_overlap.CompleteDraw(overlapping_draw));
    assert(read_overlap.FinishPresent().flags&Gate::kSourceReadOverlap);
    Gate read_concurrent;
    const auto owning_draw=read_concurrent.EnterDraw();
    const auto first_read=read_concurrent.EnterSourceRead();
    const auto second_read=read_concurrent.EnterSourceRead();
    assert(!second_read.eligible);
    assert(!read_concurrent.CompleteSourceRead(first_read));
    assert(!read_concurrent.CompleteSourceRead(second_read));
    assert(!read_concurrent.CompleteDraw(owning_draw));
    assert(read_concurrent.FinishPresent().flags&Gate::kSourceReadOverlap);
    Gate read_present;
    const auto old_draw=read_present.EnterDraw();
    const auto old_read=read_present.EnterSourceRead();
    const auto old_frame=read_present.FinishPresent();
    assert(old_frame.flags&Gate::kSourceReadInFlight);
    read_present.ReopenAfterPhysicalPresentReturn();
    assert(!read_present.CompleteSourceRead(old_read));
    assert(!read_present.CompleteDraw(old_draw));
    assert(read_present.FinishPresent().flags&Gate::kCrossPresent);
    Gate read_reset;
    const auto reset_draw=read_reset.EnterDraw();
    const auto reset_read=read_reset.EnterSourceRead();
    read_reset.Reset();
    assert(!read_reset.CompleteSourceRead(reset_read));
    assert(!read_reset.CompleteDraw(reset_draw));
    assert(read_reset.FinishPresent().flags&Gate::kCrossPresent);
    Gate missing_read;
    const auto standalone=missing_read.EnterSourceRead();
    assert(!standalone.eligible);
    assert(!missing_read.CompleteSourceRead(standalone));
    assert(missing_read.FinishPresent().flags&Gate::kSourceReadOverlap);
    // Slow genuine SetVSConstant/SetTransform COM straddling Draw:
    // post-COM lineage counts alone are unsafe even if returning success.
    Gate overlap;
    auto slow=overlap.EnterWrite();
    auto conflict=overlap.EnterDraw();
    assert(!overlap.MayForwardCpuWorld());
    overlap.CompleteWrite(slow,true,true);
    assert(!overlap.CompleteDraw(conflict));
    assert(overlap.FinishPresent().flags&Gate::kOverlap);
    // Two concurrent original hardware Draws also violate the single
    // camera/source program transaction even without Set* mutation.
    Gate dual_draw;
    auto d1=dual_draw.EnterDraw();
    auto d2=dual_draw.EnterDraw();
    assert(d1.eligible && !d2.eligible);
    assert(!dual_draw.MayForwardCpuWorld());
    assert(!dual_draw.CompleteDraw(d1));
    assert(!dual_draw.CompleteDraw(d2));
    auto dual=dual_draw.FinishPresent();
    assert(dual.flags&Gate::kOverlap);
    assert(dual.draws==2u);
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
