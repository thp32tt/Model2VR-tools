#include "../src/common/daytona_wizard_health.hpp"
#include <cassert>
#include <cstring>
int main(){
    using namespace m2vr::wizard;
    assert(Classify(false,false,false,false)==Status::TrackingLost);
    assert(Classify(true,false,false,false)==Status::AwaitingSource);
    assert(Classify(true,true,true,false)==Status::DiagnosticOnly);
    assert(Classify(true,true,true,true)==Status::LiveGameStereo);
    assert(Classify(true,false,true,true)==Status::SourceStale);
    assert(std::strcmp(Name(Status::LiveGameStereo),"LIVE_GAME_STEREO")==0);
    RuntimeGate gate;
    assert(gate.Transition(Status::AwaitingSource));
    assert(!gate.Transition(Status::AwaitingSource));
    assert(gate.Transition(Status::LiveGameStereo));
    assert(gate.CaptureEdge(true));
    assert(!gate.CaptureEdge(true));
    assert(!gate.CaptureEdge(false));
    assert(gate.CaptureEdge(true));
}
