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
    CHECK(gate.NotifyLocalChange(300));
    CHECK(gate.NotifyLocalChange(325));
    CHECK(gate.OnPredictedFrame(301)==ReferenceSpaceFrame::SuppressPending);
    CHECK(gate.OnPredictedFrame(325)==ReferenceSpaceFrame::SuppressTransition);
    CHECK(gate.Epoch()==3);
    CHECK(gate.OnPredictedFrame(326)==ReferenceSpaceFrame::Ready);
    CHECK(gate.NotifyLocalChange(0));
    CHECK(gate.OnPredictedFrame(0)==ReferenceSpaceFrame::SuppressPending);
    CHECK(gate.OnPredictedFrame(327)==ReferenceSpaceFrame::SuppressTransition);
    CHECK(gate.Epoch()==4);
    CHECK(gate.OnPredictedFrame(328)==ReferenceSpaceFrame::Ready);
    CHECK(gate.BeginSession() && gate.Epoch()==5);
    CHECK(gate.NotifyLocalChange(999));
    CHECK(gate.OnPredictedFrame(400)==ReferenceSpaceFrame::SuppressPending);
    CHECK(gate.BeginSession() && gate.Epoch()==6 && !gate.Pending());
    CHECK(gate.OnPredictedFrame(401)==ReferenceSpaceFrame::Ready);
    CHECK(gate.NotifyLocalChange(2000));
    CHECK(gate.OnPredictedFrame(402)==ReferenceSpaceFrame::SuppressPending);
    CHECK(gate.NotifyEventsLost());
    CHECK(gate.OnPredictedFrame(403)==ReferenceSpaceFrame::SuppressTransition);
    CHECK(gate.Epoch()==7);
    CHECK(gate.OnPredictedFrame(404)==ReferenceSpaceFrame::Ready);
    return 0;
}
