#include "../src/common/daytona_eye_delivery_status.hpp"
#include <cassert>
using namespace m2vr::delivery;
int main(){
    Evidence e{};
    assert(Classify(e)==Status::NoEmulatorPid);
    e.emulator_selected=true;
    assert(Classify(e)==Status::NoProducerHealth);
    e.producer_health_fresh=true;
    assert(Classify(e)==Status::NoFrameTransport);
    e.frame_transport_open=true;
    assert(Classify(e)==Status::AwaitingGameFrame);
    e.missing_original_pose_tag=true;
    assert(Classify(e)==Status::MissingOriginalPoseTag);
    e.missing_original_pose_tag=false;
    e.expired_original_pose_tag=true;
    assert(Classify(e)==Status::ExpiredOriginalPoseTag);
    e.expired_original_pose_tag=false;
    e.producer_reason_mask=2;
    assert(Classify(e)==Status::ProducerRejected);
    e.producer_reason_mask=0;
    e.ever_accepted_pair=true;
    assert(Classify(e)==Status::SourcePairStale);
    e.paired_frame_fresh=true;
    assert(Classify(e)==Status::HostEyeSizeMismatch);
    e.swapchain_compatible=true;
    assert(Classify(e)==Status::DiagnosticOnly);
    e.pair_is_original_mono=true;
    assert(Classify(e)==Status::LiveOriginalMonoFallback);
    e.pair_is_original_mono=false;e.pair_is_game=true;
    assert(Classify(e)==Status::LiveGameStereo);
    e.pair_is_verified_cpu_world=true;
    assert(Classify(e)==Status::LiveVerifiedCpuWorld);
    e.pair_is_verified_cpu_world=false;
    // Valid game stereo must not become "live" if producer later rejects
    // newer original Present: the host must set paired_frame_fresh=false.
    e.paired_frame_fresh=false;e.producer_reason_mask=256;
    assert(Classify(e)==Status::ProducerRejected);
    e.producer_reason_mask=512u;
    assert(Classify(e)==Status::SourcePairStale);
    e.mono_anchor_rejected=true;
    assert(Classify(e)==Status::MonoQuadAnchorRejected);
    e.mono_anchor_rejected=false;
    TransitionGate gate;
    assert(gate.Changed(Status::NoFrameTransport));
    assert(!gate.Changed(Status::NoFrameTransport));
    assert(gate.Changed(Status::LiveGameStereo));
    gate.Reset();assert(gate.Changed(Status::LiveGameStereo));
    return 0;
}
