#include "../src/common/daytona_xr_layer_commit_gate.hpp"
#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"final xr submit gate %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
    using namespace m2vr;
    using namespace m2vr::xrcommit;
    using namespace m2vr::transport;
    LARGE_INTEGER t{},frequency{};
    CHECK(QueryPerformanceFrequency(&frequency) && frequency.QuadPart>0);
    CHECK(QueryPerformanceCounter(&t) && t.QuadPart>0);
    const auto hz=static_cast<std::uint64_t>(frequency.QuadPart);
    const auto now=static_cast<std::uint64_t>(t.QuadPart);
    StereoFrameGate accepted;
    Snapshot eyes{};
    eyes.kind=FrameKind::GameStereo;eyes.frame_id=10u;
    eyes.timestamp_qpc=now;eyes.pose_sequence=77u;
    eyes.eyes[0].resize(kBytesPerEye);
    eyes.eyes[1].resize(kBytesPerEye);
    CHECK(accepted.Accept(eyes,now,hz));
    producerhealth::Snapshot health{};
    health.frame_id=10u;health.timestamp_qpc=now;
    health.reason_mask=0u;health.verified_scene_draws=3u;
    health.mirrored_draws=3u;health.family_draws={1u,2u,0u,0u};
    auto verify=[&](bool alive,bool pose,bool read=true){
        return VerifyOriginalFrameAtEnd(alive,accepted,eyes,pose,now,hz,
                                       health,read);
    };
    CHECK(verify(true,true).submit);
    CHECK(verify(true,true).reason==Reject::None);
    CHECK(Name(Reject::None)[0]=='O');
    CHECK(Name(Reject::SourceDead)[0]=='O');
    CHECK(Name(Reject::StalePixels)[0]=='O');
    CHECK(Name(Reject::NewerFailedPresent)[0]=='N');
    CHECK(Name(Reject::ContradictoryReceipt)[0]=='O');
    CHECK(!verify(false,true).submit);
    CHECK(verify(false,true).reason==Reject::SourceDead);
    CHECK(!verify(true,false).submit);
    CHECK(verify(true,false).reason==Reject::PoseUnproven);
    CHECK(verify(true,true,false).reason==Reject::MissingReceipt);
    health.frame_id=9u;
    CHECK(verify(true,true).reason==Reject::MissingReceipt);
    health.frame_id=10u;health.timestamp_qpc=now-1u;
    CHECK(verify(true,true).reason==Reject::ContradictoryReceipt);
    health.timestamp_qpc=now;
    health.reason_mask=producerhealth::ReadbackFailed;
    CHECK(verify(true,true).reason==Reject::ContradictoryReceipt);
    health.reason_mask=0u;
    // Original physical Present 11 completes while OpenXR uploads frame 10.
    // A newer verified success may hold frame 10 for a bounded XR tick.
    health.frame_id=11u;
    CHECK(verify(true,true).submit);
    health.timestamp_qpc=now-1u;
    CHECK(verify(true,true).reason==Reject::ContradictoryReceipt);
    health.timestamp_qpc=now;
    // No scene/Draw cannot be a harmless newer source success.
    health.verified_scene_draws=0u;
    CHECK(verify(true,true).reason==Reject::NewerFailedPresent);
    health.verified_scene_draws=3u;
    health.reason_mask=producerhealth::OriginalPresentFailed;
    CHECK(verify(true,true).reason==Reject::NewerFailedPresent);
    health.reason_mask=producerhealth::OriginalMonoFallback;
    CHECK(verify(true,true).reason==Reject::NewerFailedPresent);
    // Original game 2D->mono is one RGBA source, NOT two projected eyes.
    Snapshot mono=eyes;
    mono.frame_id=11u;mono.pose_sequence=0u;
    mono.kind=FrameKind::CompactMono;mono.eyes[1].clear();
    StereoFrameGate mono_gate;
    CHECK(mono_gate.Accept(mono,now,hz));
    health.frame_id=11u;health.reason_mask=producerhealth::OriginalMonoFallback;
    CHECK(VerifyOriginalFrameAtEnd(true,mono_gate,mono,false,now,hz,
            health,true).submit);
    mono.pose_sequence=77u;
    CHECK(VerifyOriginalFrameAtEnd(true,mono_gate,mono,false,now,hz,
            health,true).reason==Reject::PoseUnproven);
    mono.pose_sequence=0u;
    // Later published mono can safely hold a previous mono for the short
    // original game backbuffer interval, but not a broken source HRESULT.
    health.frame_id=12u;
    CHECK(VerifyOriginalFrameAtEnd(true,mono_gate,mono,false,now,hz,
            health,true).submit);
    health.reason_mask=producerhealth::ReadbackFailed;
    CHECK(VerifyOriginalFrameAtEnd(true,mono_gate,mono,false,now,hz,
            health,true).reason==Reject::NewerFailedPresent);
    // Freshness rechecked AFTER swapchain upload, not the earlier copy time.
    health.reason_mask=producerhealth::OriginalMonoFallback;
    CHECK(VerifyOriginalFrameAtEnd(true,mono_gate,mono,false,
            now+hz, hz, health,true).reason==Reject::StalePixels);
    // Diagnostic source never claims a genuine Daytona Present, but still
    // needs a live source process and fresh coherent original IPC buffer.
    StereoFrameGate diag_gate;
    Snapshot diag=eyes;diag.frame_id=20u;diag.kind=FrameKind::Diagnostic;
    diag.pose_sequence=0u;
    CHECK(diag_gate.Accept(diag,now,hz));
    CHECK(VerifyOriginalFrameAtEnd(true,diag_gate,diag,false,now,hz,
        {},false).submit);
    CHECK(VerifyOriginalFrameAtEnd(false,diag_gate,diag,false,now,hz,
        {},false).reason==Reject::SourceDead);
    diag.eyes[1].clear();
    CHECK(VerifyOriginalFrameAtEnd(true,diag_gate,diag,false,now,hz,
        {},false).reason==Reject::PayloadMismatch);
    return 0;
}
