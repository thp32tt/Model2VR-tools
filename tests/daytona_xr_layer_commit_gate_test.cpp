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
                                       health,read,now,now);
    };
    CHECK(verify(true,true).submit);
    CHECK(verify(true,true).reason==Reject::None);
    // Regression: after accepting a genuine physical x86 frame, a stale
    // or substituted same-ID/same-kind eye payload with different QPC
    // must not reach the compositor even if the receipt is made to match.
    // Previously final submit matched only frame ID, kind and byte sizes.
    CHECK(accepted.MatchesAcceptedMetadata(eyes));
    eyes.timestamp_qpc=now+1u;
    health.timestamp_qpc=now+1u;
    CHECK(!accepted.MatchesAcceptedMetadata(eyes));
    CHECK(verify(true,true).reason==Reject::PayloadMismatch);
    eyes.timestamp_qpc=now;
    health.timestamp_qpc=now;
    // Also bind the CURRENT source-eye pose tag, not merely the maximum
    // tracked tag historically observed before mono/stereo transitions.
    eyes.pose_sequence=78u;
    CHECK(!accepted.MatchesAcceptedMetadata(eyes));
    CHECK(verify(true,true).reason==Reject::PayloadMismatch);
    eyes.pose_sequence=77u;
    CHECK(accepted.MatchesAcceptedMetadata(eyes));
    CHECK(verify(true,true).submit);
    CHECK(VerifyOriginalFrameAtEnd(
        true,accepted,eyes,true,now,hz,health,true,now+1u,now).reason==
        Reject::PreviousHostGeneration);
    CHECK(VerifyOriginalFrameAtEnd(
        true,accepted,eyes,true,now,hz,health,true,0u,now).reason==
        Reject::PreviousHostGeneration);
    CHECK(Name(Reject::PreviousHostGeneration)[0]=='O');
    CHECK(Name(Reject::PreviousXrSessionGeneration)[0]=='O');
    // A fresh buffer from a previous XR session in the same host process
    // must not be submitted under the new session's display time.
    CHECK(VerifyOriginalFrameAtEnd(
        true,accepted,eyes,true,now,hz,health,true,now,now+1u).reason==
        Reject::PreviousXrSessionGeneration);
    CHECK(VerifyOriginalFrameAtEnd(
        true,accepted,eyes,true,now,hz,health,true,now,0u).reason==
        Reject::PreviousXrSessionGeneration);
    CHECK(xrepoch::WithinActiveSession(now,now,now));
    CHECK(!xrepoch::WithinActiveSession(now,now,now+1u));
    CHECK(!xrepoch::WithinActiveSession(now,now+1u,now));
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
    // Newer successful CPU world source is NOT the same kind as a
    // previously accepted speculative GPU GameStereo pair.
    health.reason_mask=0u;health.mirrored_draws=0u;
    health.family_draws={0u,0u,0u,0u};
    CHECK(verify(true,true).reason==Reject::NewerNativeSourceKind);
    health.mirrored_draws=3u;health.family_draws={1u,2u,0u,0u};
    CHECK(verify(true,true).submit);
    CHECK(Name(Reject::NewerNativeSourceKind)[0]=='N');
    // Original game 2D->mono is one RGBA source, NOT two projected eyes.
    Snapshot mono=eyes;
    mono.frame_id=11u;mono.pose_sequence=0u;
    mono.kind=FrameKind::CompactMono;mono.eyes[1].clear();
    StereoFrameGate mono_gate;
    CHECK(mono_gate.Accept(mono,now,hz));
    health.frame_id=11u;health.reason_mask=producerhealth::OriginalMonoFallback;
    CHECK(VerifyOriginalFrameAtEnd(true,mono_gate,mono,false,now,hz,
            health,true,now,now).submit);
    CHECK(mono_gate.MatchesAcceptedMetadata(mono));
    // Even an otherwise verified original compact-mono backbuffer is
    // never authorized as a different same-ID QPC source after upload.
    mono.timestamp_qpc=now+1u;
    CHECK(VerifyOriginalFrameAtEnd(true,mono_gate,mono,false,now,hz,
            health,true,now,now).reason==Reject::PayloadMismatch);
    mono.timestamp_qpc=now;
    mono.pose_sequence=77u;
    // The final exact-source binding rejects substituted tagged mono
    // metadata even before the generic unproven-pose classification.
    CHECK(VerifyOriginalFrameAtEnd(true,mono_gate,mono,false,now,hz,
            health,true,now,now).reason==Reject::PayloadMismatch);
    mono.pose_sequence=0u;
    // Later published mono can safely hold a previous mono for the short
    // original game backbuffer interval, but not a broken source HRESULT.
    health.frame_id=12u;
    CHECK(VerifyOriginalFrameAtEnd(true,mono_gate,mono,false,now,hz,
            health,true,now,now).submit);
    health.reason_mask=producerhealth::ReadbackFailed;
    CHECK(VerifyOriginalFrameAtEnd(true,mono_gate,mono,false,now,hz,
            health,true,now,now).reason==Reject::NewerFailedPresent);
    // Mono->authentic native stereo recovers while the old cinema packet
    // remains QPC-fresh: its 250ms cache must be retired right away.
    health.reason_mask=0u;health.verified_scene_draws=3u;
    health.mirrored_draws=3u;health.family_draws={1u,2u,0u,0u};
    CHECK(VerifyOriginalFrameAtEnd(true,mono_gate,mono,false,now,hz,
            health,true,now,now).reason==Reject::NewerNativeSourceKind);
    health.mirrored_draws=0u;health.family_draws={0u,0u,0u,0u};
    CHECK(VerifyOriginalFrameAtEnd(true,mono_gate,mono,false,now,hz,
            health,true,now,now).reason==Reject::NewerNativeSourceKind);
    // Freshness rechecked AFTER swapchain upload, not the earlier copy time.
    health.reason_mask=producerhealth::OriginalMonoFallback;
    CHECK(VerifyOriginalFrameAtEnd(true,mono_gate,mono,false,
            now+hz, hz, health,true,now,now).reason==Reject::StalePixels);
    // Diagnostic source never claims a genuine Daytona Present, but still
    // needs a live source process and fresh coherent original IPC buffer.
    StereoFrameGate diag_gate;
    Snapshot diag=eyes;diag.frame_id=20u;diag.kind=FrameKind::Diagnostic;
    diag.pose_sequence=0u;
    CHECK(diag_gate.Accept(diag,now,hz));
    CHECK(VerifyOriginalFrameAtEnd(true,diag_gate,diag,false,now,hz,
        {},false,now,now).submit);
    CHECK(VerifyOriginalFrameAtEnd(false,diag_gate,diag,false,now,hz,
        {},false,now,now).reason==Reject::SourceDead);
    diag.eyes[1].clear();
    CHECK(VerifyOriginalFrameAtEnd(true,diag_gate,diag,false,now,hz,
        {},false,now,now).reason==Reject::PayloadMismatch);
    return 0;
}
