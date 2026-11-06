#include "../src/common/model2_reference_space_epoch.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"LOCAL epoch line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using m2vr::pose::ReferenceSpaceEpochGate;
using m2vr::pose::ReferenceSpaceFrame;
int main(){
    ReferenceSpaceEpochGate gate;
    CHECK(gate.OnPredictedFrame(100)==ReferenceSpaceFrame::Exhausted);
    CHECK(gate.BeginSession() && gate.Epoch()==1);
    CHECK(gate.OnPredictedFrame(100)==ReferenceSpaceFrame::Ready);
    CHECK(gate.NotifyLocalChange(200) && gate.Pending());
    CHECK(gate.OnPredictedFrame(199)==ReferenceSpaceFrame::SuppressPending);
    CHECK(gate.Epoch()==1);
    CHECK(gate.OnPredictedFrame(200)==ReferenceSpaceFrame::SuppressTransition);
    CHECK(gate.Epoch()==2 && !gate.Pending());
    CHECK(gate.OnPredictedFrame(201)==ReferenceSpaceFrame::Ready);
    // Two independent future LOCAL changes cannot be collapsed into
    // the LATER changeTime. A source frame at 301 might already belong
    // to the new origin scheduled at 300. Fail closed for this XR session.
    CHECK(gate.NotifyLocalChange(300));
    CHECK(gate.Pending());
    CHECK(gate.NotifyLocalChange(325));
    CHECK(gate.OverlappingChangeTime() && !gate.Pending());
    CHECK(gate.Epoch()==2u);
    CHECK(gate.OnPredictedFrame(299)==ReferenceSpaceFrame::Exhausted);
    CHECK(gate.OnPredictedFrame(301)==ReferenceSpaceFrame::Exhausted);
    CHECK(gate.OnPredictedFrame(325)==ReferenceSpaceFrame::Exhausted);
    CHECK(!gate.NotifyLocalChange(600));
    CHECK(gate.BeginSession() && gate.Epoch()==3u);
    CHECK(!gate.OverlappingChangeTime() && !gate.Pending());
    CHECK(gate.OnPredictedFrame(326)==ReferenceSpaceFrame::Ready);
    // A missing transition time could hide an arbitrarily late LOCAL
    // origin change. Never switch on the next guessed positive frame.
    CHECK(gate.NotifyLocalChange(0) && gate.ChangeTimeUnknown());
    CHECK(!gate.Pending());
    CHECK(gate.OnPredictedFrame(0)==ReferenceSpaceFrame::Exhausted);
    CHECK(gate.OnPredictedFrame(327)==ReferenceSpaceFrame::Exhausted);
    CHECK(gate.OnPredictedFrame(5000)==ReferenceSpaceFrame::Exhausted);
    CHECK(!gate.NotifyLocalChange(6000)); // a later known event cannot heal it
    CHECK(gate.Epoch()==3);
    CHECK(gate.BeginSession() && gate.Epoch()==4 && !gate.ChangeTimeUnknown());
    CHECK(gate.OnPredictedFrame(328)==ReferenceSpaceFrame::Ready);
    // An unknown second event cannot be overwritten by an earlier
    // pending positive changeTime.
    CHECK(gate.NotifyLocalChange(2000));
    CHECK(gate.NotifyLocalChange(-1) && gate.ChangeTimeUnknown());
    CHECK(!gate.Pending());
    CHECK(gate.OnPredictedFrame(2000)==ReferenceSpaceFrame::Exhausted);
    CHECK(gate.BeginSession() && gate.Epoch()==5);
    CHECK(gate.NotifyLocalChange(999));
    CHECK(gate.OnPredictedFrame(400)==ReferenceSpaceFrame::SuppressPending);
    CHECK(gate.BeginSession() && gate.Epoch()==6 && !gate.Pending());
    CHECK(gate.OnPredictedFrame(401)==ReferenceSpaceFrame::Ready);
    CHECK(gate.NotifyLocalChange(2000));
    CHECK(gate.OnPredictedFrame(402)==ReferenceSpaceFrame::SuppressPending);
    CHECK(gate.NotifyEventsLost() && gate.EventsLost());
    CHECK(!gate.Pending());
    CHECK(gate.OnPredictedFrame(403)==ReferenceSpaceFrame::Exhausted);
    CHECK(gate.OnPredictedFrame(2000)==ReferenceSpaceFrame::Exhausted);
    CHECK(gate.OnPredictedFrame(9000)==ReferenceSpaceFrame::Exhausted);
    CHECK(!gate.NotifyLocalChange(10000));
    CHECK(gate.BeginSession() && gate.Epoch()==7 && !gate.EventsLost());
    CHECK(gate.OnPredictedFrame(9001)==ReferenceSpaceFrame::Ready);
    // Independent XR sessions: real predicted display time must advance.
    // Duplicate time during a pending LOCAL change is not a safe way to
    // call that frame Ready and cannot promote a guessed new origin.
    ReferenceSpaceEpochGate duplicate;
    CHECK(duplicate.BeginSession() && duplicate.Epoch()==1u);
    CHECK(duplicate.OnPredictedFrame(100)==ReferenceSpaceFrame::Ready);
    CHECK(duplicate.NotifyLocalChange(200));
    CHECK(duplicate.OnPredictedFrame(199)==ReferenceSpaceFrame::SuppressPending);
    CHECK(duplicate.OnPredictedFrame(199)==ReferenceSpaceFrame::Exhausted);
    CHECK(duplicate.TimeDiscontinuity() && !duplicate.Pending());
    CHECK(duplicate.Epoch()==1u && !duplicate.NotifyLocalChange(300));
    CHECK(duplicate.OnPredictedFrame(500)==ReferenceSpaceFrame::Exhausted);
    CHECK(duplicate.BeginSession() && duplicate.Epoch()==2u);
    CHECK(!duplicate.TimeDiscontinuity());
    CHECK(duplicate.OnPredictedFrame(1)==ReferenceSpaceFrame::Ready);
    // A reverse timestamp after a real LOCAL boundary cannot reuse the
    // new epoch for old stereo eyes, even when OpenXR tracking is valid.
    ReferenceSpaceEpochGate rollback;
    CHECK(rollback.BeginSession());
    CHECK(rollback.OnPredictedFrame(500)==ReferenceSpaceFrame::Ready);
    CHECK(rollback.NotifyLocalChange(600));
    CHECK(rollback.OnPredictedFrame(600)==ReferenceSpaceFrame::SuppressTransition);
    CHECK(rollback.Epoch()==2u);
    CHECK(rollback.OnPredictedFrame(599)==ReferenceSpaceFrame::Exhausted);
    CHECK(rollback.TimeDiscontinuity());
    CHECK(rollback.OnPredictedFrame(601)==ReferenceSpaceFrame::Exhausted);
    CHECK(rollback.BeginSession() && rollback.Epoch()==3u);
    CHECK(rollback.OnPredictedFrame(1)==ReferenceSpaceFrame::Ready);
    // Missing/invalid display time on the FIRST frame is also forbidden:
    // there is no valid xrLocateViews time to associate with a CPU frame.
    ReferenceSpaceEpochGate missing;
    CHECK(missing.BeginSession());
    CHECK(missing.OnPredictedFrame(0)==ReferenceSpaceFrame::Exhausted);
    CHECK(missing.TimeDiscontinuity());
    CHECK(missing.OnPredictedFrame(100)==ReferenceSpaceFrame::Exhausted);
    CHECK(missing.BeginSession());
    CHECK(missing.OnPredictedFrame(100)==ReferenceSpaceFrame::Ready);
    CHECK(missing.OnPredictedFrame(-10)==ReferenceSpaceFrame::Exhausted);
    // A LOCAL change delivered after its effective time has already been
    // processed must not retroactively label old game eyes as a new origin.
    ReferenceSpaceEpochGate delayed;
    CHECK(delayed.BeginSession() && delayed.Epoch()==1u);
    CHECK(delayed.OnPredictedFrame(100)==ReferenceSpaceFrame::Ready);
    CHECK(delayed.NotifyLocalChange(100) && delayed.LateChangeTime());
    CHECK(!delayed.Pending() && delayed.Epoch()==1u);
    CHECK(delayed.OnPredictedFrame(101)==ReferenceSpaceFrame::Exhausted);
    CHECK(!delayed.NotifyLocalChange(200));
    CHECK(delayed.OnPredictedFrame(999)==ReferenceSpaceFrame::Exhausted);
    CHECK(delayed.BeginSession() && delayed.Epoch()==2u);
    CHECK(!delayed.LateChangeTime());
    CHECK(delayed.OnPredictedFrame(1)==ReferenceSpaceFrame::Ready);
    // A strictly older event is also late; another pending future change
    // must not mask that we already passed an independent LOCAL boundary.
    ReferenceSpaceEpochGate two_events;
    CHECK(two_events.BeginSession());
    CHECK(two_events.OnPredictedFrame(100)==ReferenceSpaceFrame::Ready);
    CHECK(two_events.NotifyLocalChange(250));
    CHECK(two_events.OnPredictedFrame(150)==ReferenceSpaceFrame::SuppressPending);
    CHECK(two_events.NotifyLocalChange(149) && two_events.LateChangeTime());
    CHECK(!two_events.Pending() && two_events.Epoch()==1u);
    CHECK(two_events.OnPredictedFrame(250)==ReferenceSpaceFrame::Exhausted);
    CHECK(two_events.BeginSession() && two_events.Epoch()==2u);
    CHECK(two_events.OnPredictedFrame(1)==ReferenceSpaceFrame::Ready);
    // Two notifications with exactly the same changeTime are not
    // automatically the same reference-space transform. Without transform
    // identity in this gate, the second transition remains ambiguous.
    ReferenceSpaceEpochGate same_effective_time;
    CHECK(same_effective_time.BeginSession());
    CHECK(same_effective_time.OnPredictedFrame(100)==ReferenceSpaceFrame::Ready);
    CHECK(same_effective_time.NotifyLocalChange(200));
    CHECK(same_effective_time.NotifyLocalChange(200));
    CHECK(same_effective_time.OverlappingChangeTime());
    CHECK(same_effective_time.OnPredictedFrame(200)==ReferenceSpaceFrame::Exhausted);
    CHECK(same_effective_time.BeginSession());
    CHECK(!same_effective_time.OverlappingChangeTime());
    CHECK(same_effective_time.OnPredictedFrame(1)==ReferenceSpaceFrame::Ready);
    // A second EARLIER, but still future, change is equally ambiguous;
    // do not publish at the later stored boundary or the earlier one.
    ReferenceSpaceEpochGate reverse_order;
    CHECK(reverse_order.BeginSession());
    CHECK(reverse_order.OnPredictedFrame(100)==ReferenceSpaceFrame::Ready);
    CHECK(reverse_order.NotifyLocalChange(300));
    CHECK(reverse_order.NotifyLocalChange(250));
    CHECK(reverse_order.OverlappingChangeTime());
    CHECK(reverse_order.OnPredictedFrame(200)==ReferenceSpaceFrame::Exhausted);
    CHECK(reverse_order.OnPredictedFrame(301)==ReferenceSpaceFrame::Exhausted);
    CHECK(reverse_order.BeginSession());
    CHECK(reverse_order.OnPredictedFrame(1)==ReferenceSpaceFrame::Ready);
    // A single known, non-overlapping pending change retains the original
    // exact-time suppression and fresh LOCAL generation behavior.
    // A future timestamp that was not passed is still eligible for the
    // usual pending / exact-changeTime suppression and fresh LOCAL epoch.
    CHECK(two_events.NotifyLocalChange(200));
    CHECK(two_events.OnPredictedFrame(199)==ReferenceSpaceFrame::SuppressPending);
    CHECK(two_events.OnPredictedFrame(200)==ReferenceSpaceFrame::SuppressTransition);
    CHECK(two_events.OnPredictedFrame(201)==ReferenceSpaceFrame::Ready);
    return 0;
}
