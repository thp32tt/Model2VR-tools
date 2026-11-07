#include "../src/common/daytona_source_handoff_gate.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(v) do { if(!(v)){std::fprintf(stderr,"source handoff %d: %s\n",__LINE__,#v);std::exit(1);} } while(0)
int main(){
    using namespace m2vr;
    transport::FrameStamp peek{};
    peek.frame_id=43u;peek.timestamp_qpc=5000u;
    peek.writer_pid=901u;peek.pose_sequence=18u;
    peek.kind=transport::FrameKind::GameStereo;
    transport::Snapshot source{};
    source.frame_id=peek.frame_id;source.timestamp_qpc=peek.timestamp_qpc;
    source.pose_sequence=peek.pose_sequence;source.kind=peek.kind;
    source.eyes[0].resize(transport::kBytesPerEye);
    source.eyes[1].resize(transport::kBytesPerEye);
    producerhealth::Snapshot receipt{};
    receipt.frame_id=peek.frame_id;receipt.timestamp_qpc=5100u;
    receipt.reason_mask=0u;receipt.verified_scene_draws=1u;
    receipt.mirrored_draws=2u;receipt.family_draws={1u,1u,0u,0u};
    CHECK(handoff::AdmitOriginalPresentToXr(peek,source,receipt,true,true,5000u));
    // Same pose tag + otherwise valid D3D9 Present is not current-host proof.
    CHECK(!handoff::AdmitOriginalPresentToXr(
        peek,source,receipt,true,true,5001u));
    CHECK(!handoff::AdmitOriginalPresentToXr(
        peek,source,receipt,true,true,0u));
    CHECK(!handoff::AdmitOriginalPresentToXr(peek,source,receipt,true,false,5000u));
    CHECK(!handoff::AdmitOriginalPresentToXr(peek,source,receipt,false,true,5000u));
    auto advanced=source;
    advanced.frame_id++;
    CHECK(!handoff::AdmitOriginalPresentToXr(peek,advanced,receipt,true,true,5000u));
    advanced=source;advanced.timestamp_qpc++;
    CHECK(!handoff::AdmitOriginalPresentToXr(peek,advanced,receipt,true,true,5000u));
    advanced=source;advanced.pose_sequence++;
    CHECK(!handoff::AdmitOriginalPresentToXr(peek,advanced,receipt,true,true,5000u));
    advanced=source;advanced.kind=transport::FrameKind::VerifiedCpuWorldStereo;
    CHECK(!handoff::AdmitOriginalPresentToXr(peek,advanced,receipt,true,true,5000u));
    advanced=source;advanced.eyes[1].clear();
    CHECK(!handoff::AdmitOriginalPresentToXr(peek,advanced,receipt,true,true,5000u));
    auto stale=receipt;stale.frame_id--;
    CHECK(!handoff::AdmitOriginalPresentToXr(peek,source,stale,true,true,5000u));
    stale=receipt;stale.frame_id++;
    CHECK(!handoff::AdmitOriginalPresentToXr(peek,source,stale,true,true,5000u));
    stale=receipt;stale.timestamp_qpc=4999u;
    CHECK(!handoff::AdmitOriginalPresentToXr(peek,source,stale,true,true,5000u));
    stale=receipt;stale.reason_mask=producerhealth::ReadbackFailed;
    CHECK(!handoff::AdmitOriginalPresentToXr(peek,source,stale,true,true,5000u));
    // Original mono takes exactly one x86 copy and must NEVER inherit an XR tag.
    peek.frame_id=44u;peek.timestamp_qpc=6000u;peek.pose_sequence=0u;
    peek.kind=transport::FrameKind::CompactMono;
    source.frame_id=44u;source.timestamp_qpc=6000u;source.pose_sequence=0u;
    source.kind=transport::FrameKind::CompactMono;source.eyes[1].clear();
    receipt.frame_id=44u;receipt.timestamp_qpc=6100u;
    receipt.reason_mask=producerhealth::OriginalMonoFallback;
    receipt.mirrored_draws=0u;receipt.mirrored_clears=0u;
    receipt.verified_scene_draws=0u;receipt.family_draws={};
    CHECK(handoff::AdmitOriginalPresentToXr(peek,source,receipt,true,false,5000u));
    source.pose_sequence=18u;
    CHECK(!handoff::AdmitOriginalPresentToXr(peek,source,receipt,true,true,5000u));
    source.pose_sequence=0u;source.eyes[1].resize(transport::kBytesPerEye);
    CHECK(!handoff::AdmitOriginalPresentToXr(peek,source,receipt,true,false,5000u));
    source.eyes[1].clear();
    // Explicit CPU World3D remains proof-gated at its original producer.
    peek.frame_id=45u;peek.timestamp_qpc=7000u;peek.pose_sequence=19u;
    peek.kind=transport::FrameKind::VerifiedCpuWorldStereo;
    source.frame_id=45u;source.timestamp_qpc=7000u;source.pose_sequence=19u;
    source.kind=peek.kind;source.eyes[1].resize(transport::kBytesPerEye);
    receipt.frame_id=45u;receipt.timestamp_qpc=7100u;
    receipt.reason_mask=0u;receipt.verified_scene_draws=2u;
    CHECK(handoff::AdmitOriginalPresentToXr(peek,source,receipt,true,true,5000u));
    receipt.mirrored_draws=1u;receipt.family_draws={1u,0u,0u,0u};
    CHECK(!handoff::AdmitOriginalPresentToXr(peek,source,receipt,true,true,5000u));
    // Host must not promote a malformed diagnostic or zero physical QPC.
    peek.kind=source.kind=transport::FrameKind::Diagnostic;
    peek.pose_sequence=source.pose_sequence=0u;
    CHECK(handoff::AdmitOriginalPresentToXr(peek,source,receipt,false,false,5000u));
    peek.timestamp_qpc=0u;
    CHECK(!handoff::AdmitOriginalPresentToXr(peek,source,receipt,false,false,5000u));
    return 0;
}
