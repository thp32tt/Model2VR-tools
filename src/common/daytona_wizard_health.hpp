#pragma once
// Host health classification; LIVE_GAME_STEREO is NOT verified 6DoF camera.
#include <cstdint>
namespace m2vr::wizard {
enum class Status : std::uint8_t {
    TrackingLost, AwaitingSource, DiagnosticOnly, LiveGameStereo, SourceStale
};
constexpr Status Classify(bool tracking_valid, bool paired_frame_fresh,
                          bool source_seen, bool source_is_game) noexcept {
    if(!tracking_valid)return Status::TrackingLost;
    if(!paired_frame_fresh)
        return source_seen?Status::SourceStale:Status::AwaitingSource;
    return source_is_game?Status::LiveGameStereo:Status::DiagnosticOnly;
}
constexpr const char* Name(Status status) noexcept {
    switch(status){
        case Status::TrackingLost:return "TRACKING_LOST";
        case Status::AwaitingSource:return "AWAITING_GAME_SOURCE";
        case Status::DiagnosticOnly:return "DIAGNOSTIC_NOT_GAME";
        case Status::LiveGameStereo:return "LIVE_GAME_STEREO";
        case Status::SourceStale:return "SOURCE_STALE";
    }
    return "UNKNOWN";
}
class RuntimeGate {
public:
    bool Transition(Status status) noexcept {
        if(has_status_ && status==last_)return false;
        last_=status;has_status_=true;return true;
    }
    bool CaptureEdge(bool key_down) noexcept {
        const bool triggered=key_down && !capture_key_down_;
        capture_key_down_=key_down;return triggered;
    }
private:
    Status last_=Status::AwaitingSource;
    bool has_status_=false,capture_key_down_=false;
};
}
