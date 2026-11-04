#include "../src/common/stereo_frame_policy.hpp"
#include <cstdio>
#include <cstdint>
using namespace m2vr::transport;
#define CHECK(expr) do{if(!(expr)){std::fprintf(stderr,"frame gate failed at %d: %s\n",__LINE__,#expr);return 1;}}while(0)

int main(){
    // A native GameStereo source MUST identify a pose in the x64 host ring.
    // Legacy untagged producer packets remain ABI-readable but not HMD-safe.
    CHECK(IsStereoGame(FrameKind::GameStereo));
    CHECK(IsStereoGame(FrameKind::VerifiedCpuWorldStereo));
    CHECK(!IsStereoGame(FrameKind::CompactMono));
    CHECK(SourcePoseMatchesFrame(FrameKind::VerifiedCpuWorldStereo,123u,true));
    CHECK(!SourcePoseMatchesFrame(FrameKind::VerifiedCpuWorldStereo,0u,true));
    CHECK(!SourcePoseMatchesFrame(FrameKind::VerifiedCpuWorldStereo,123u,false));
    CHECK(ValidEyePayload(FrameKind::VerifiedCpuWorldStereo,kBytesPerEye,kBytesPerEye));
    CHECK(!ValidEyePayload(FrameKind::VerifiedCpuWorldStereo,kBytesPerEye,0u));
    CHECK(SourcePoseMatchesFrame(FrameKind::GameStereo,123u,true));
    CHECK(!SourcePoseMatchesFrame(FrameKind::GameStereo,0u,true));
    CHECK(!SourcePoseMatchesFrame(FrameKind::GameStereo,123u,false));
    CHECK(SourcePoseMatchesFrame(FrameKind::Diagnostic,0u,false));
    CHECK(SourcePoseMatchesFrame(FrameKind::GameMonoFallback,0u,false));
    CHECK(SourcePoseMatchesFrame(FrameKind::CompactMono,0u,false));
    CHECK(!SourcePoseMatchesFrame(FrameKind::CompactMono,9u,true));
    CHECK(ValidEyePayload(FrameKind::CompactMono,kBytesPerEye,0u));
    CHECK(!ValidEyePayload(FrameKind::CompactMono,kBytesPerEye,kBytesPerEye));
    CHECK(!ValidEyePayload(FrameKind::GameStereo,kBytesPerEye,0u));
    CHECK(!SourcePoseMatchesFrame(FrameKind::GameMonoFallback,17u,true));
    CHECK(!SourcePoseMatchesFrame(FrameKind::Diagnostic,123u,true));
    CHECK(!SourcePoseMatchesFrame(FrameKind::Invalid,0u,false));
    CHECK(!SourcePoseMatchesFrame(FrameKind::Invalid,123u,true));
    // Synthetic QPC hz = 1000 ticks/sec; hold is exactly 125ms.
    constexpr std::uint64_t hz=1000;
    constexpr std::uint64_t t=5000;
    StereoFrameGate gate{};
    FrameStamp stamp{1,t,FrameKind::GameStereo,123};
    CHECK(gate.Decide(&stamp,t,hz)==FrameUse::ReadNew);
    CHECK(gate.Decide(nullptr,t,hz)==FrameUse::Missing);
    Snapshot snap{};
    snap.frame_id=stamp.frame_id;
    snap.timestamp_qpc=stamp.timestamp_qpc;
    snap.kind=stamp.kind;
    snap.eyes[0].resize(kBytesPerEye,0x18);
    snap.eyes[1].resize(kBytesPerEye,0xD8);
    CHECK(gate.Accept(snap,t,hz));
    CHECK(gate.id()==1);
    CHECK(gate.kind()==FrameKind::GameStereo);
    CHECK(gate.Decide(&stamp,t+16,hz)==FrameUse::HoldCached);
    CHECK(gate.Decide(nullptr,t+100,hz)==FrameUse::HoldCached);
    CHECK(gate.Decide(&stamp,t+125,hz)==FrameUse::HoldCached);
    CHECK(gate.Decide(&stamp,t+126,hz)==FrameUse::Missing);
    CHECK(!gate.Cached(t+126,hz));
    CHECK(gate.Decide(nullptr,t+126,hz)==FrameUse::Missing);
    CHECK(gate.Decide(&stamp,t-1,hz)==FrameUse::Missing);
    CHECK(gate.Decide(&stamp,t,0)==FrameUse::Missing);
    // A new pair must carry its own full left/right pixels. Cannot relabel
    // a cached old pair by changing just its frame ID.
    stamp={2,t+130,FrameKind::GameStereo,123};
    CHECK(gate.Decide(&stamp,t+131,1000)==FrameUse::ReadNew);
    Snapshot partial{};
    partial.frame_id=2;
    partial.timestamp_qpc=t+130;
    partial.kind=FrameKind::GameStereo;
    partial.eyes[0].resize(kBytesPerEye);
    CHECK(!gate.Accept(partial,t+131,1000));
    CHECK(gate.id()==1);
    partial.eyes[1].resize(kBytesPerEye);
    CHECK(gate.Accept(partial,t+131,1000));
    CHECK(gate.id()==2);
    CHECK(!gate.Accept(partial,t+132,1000)); // duplicate cannot advance
    CHECK(gate.Decide(&stamp,t+132,1000)==FrameUse::HoldCached);

    FrameStamp diag{3,t+135,FrameKind::Diagnostic,123};
    CHECK(gate.Decide(&diag,t+136,1000)==FrameUse::ReadNew);
    snap.frame_id=3;
    snap.timestamp_qpc=t+135;
    snap.kind=FrameKind::Diagnostic;
    CHECK(gate.Accept(snap,t+136,1000));
    CHECK(gate.kind()==FrameKind::Diagnostic);
    FrameStamp invalid{4,t+137,FrameKind::Invalid,123};
    CHECK(gate.Decide(&invalid,t+138,1000)==FrameUse::HoldCached);
    FrameStamp regressed{1,t+138,FrameKind::GameStereo,123};
    CHECK(gate.Decide(&regressed,t+139,1000)==FrameUse::Missing);
    gate.ExpireKeepFrameId();
    CHECK(gate.id()==3);
    CHECK(gate.kind()==FrameKind::Invalid);
    CHECK(!gate.Cached(t+140,1000));
    CHECK(gate.Decide(&diag,t+140,1000)==FrameUse::Missing);
    CHECK(gate.Decide(&regressed,t+140,1000)==FrameUse::Missing);
    FrameStamp recovered{4,t+141,FrameKind::GameStereo,123};
    CHECK(gate.Decide(&recovered,t+142,1000)==FrameUse::ReadNew);
    snap.frame_id=4;snap.timestamp_qpc=t+141;
    snap.kind=FrameKind::GameStereo;
    CHECK(gate.Accept(snap,t+142,1000));
    CHECK(gate.id()==4);
    // Monotonic fallback-to-true-stereo transitions preserve the IPC frame
    // gate, without treating the cloned 2D image as native binocular.
    FrameStamp mono{5,t+145,FrameKind::GameMonoFallback,123};
    CHECK(gate.Decide(&mono,t+146,hz)==FrameUse::ReadNew);
    snap.frame_id=5;snap.timestamp_qpc=t+145;
    snap.kind=FrameKind::GameMonoFallback;
    CHECK(gate.Accept(snap,t+146,hz));
    CHECK(gate.kind()==FrameKind::GameMonoFallback);
    FrameStamp compact{6,t+147,FrameKind::CompactMono,123};
    CHECK(gate.Decide(&compact,t+148,hz)==FrameUse::ReadNew);
    snap.frame_id=6;snap.timestamp_qpc=t+147;
    snap.kind=FrameKind::CompactMono;
    CHECK(!gate.Accept(snap,t+148,hz)); // can't carry stale stereo eye
    snap.eyes[1].clear();
    CHECK(gate.Accept(snap,t+148,hz));
    CHECK(gate.kind()==FrameKind::CompactMono);
    CHECK(gate.Cached(t+147+250,hz));
    CHECK(!gate.Cached(t+147+251,hz));
    CHECK(gate.Decide(&compact,t+147+250,hz)==FrameUse::HoldCached);
    CHECK(gate.Decide(&compact,t+147+251,hz)==FrameUse::Missing);
    // Native stereo expiry remains 125ms; only explicit 2D cinema
    // kinds are allowed the more forgiving GPU-readback hold window.
    FrameStamp live_stereo{7,t+450,FrameKind::GameStereo,123};
    CHECK(gate.Decide(&live_stereo,t+451,hz)==FrameUse::ReadNew);
    snap.frame_id=7;snap.timestamp_qpc=t+450;
    snap.kind=FrameKind::GameStereo;
    snap.eyes[1].resize(kBytesPerEye);
    CHECK(gate.Accept(snap,t+451,hz));
    CHECK(gate.Cached(t+450+125,hz));
    CHECK(!gate.Cached(t+450+126,hz));
    gate.Reset();
    CHECK(gate.id()==0);
    CHECK(!gate.Cached(t+140,1000));
    CHECK(gate.Decide(&stamp,t+131,1000)==FrameUse::ReadNew);
    return 0;
}
