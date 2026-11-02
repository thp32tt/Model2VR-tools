#include "../src/common/stereo_frame_policy.hpp"
#include <cstdio>
#include <cstdint>
using namespace m2vr::transport;
#define CHECK(expr) do{if(!(expr)){std::fprintf(stderr,"frame gate failed at %d: %s\n",__LINE__,#expr);return 1;}}while(0)

int main(){
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
    gate.Reset();
    CHECK(gate.id()==0);
    CHECK(!gate.Cached(t+140,1000));
    CHECK(gate.Decide(&stamp,t+131,1000)==FrameUse::ReadNew);
    return 0;
}
