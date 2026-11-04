#pragma once
// Original projected D3D9 source camera continuity across Draws of the SAME
// physical Present. Model matrices may change per object; optical projection
// and viewport cannot switch to a mirror/preview camera under one XR pose.
// Not gameplay-world authentication or calibration.
#include "model2_camera_projection_fit.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace m2vr::vrbridge {
struct OriginalProjectionViewport {
    std::uint32_t x=0,y=0,width=0,height=0;
    constexpr bool operator==(const OriginalProjectionViewport&)const=default;
};
enum class OriginalCameraPassResult : std::uint32_t {
    Initial=0, Continuation=1, Invalid=2, FrameChanged=3,
    CallerChanged=4, ViewportChanged=5, DepthChanged=6, OpticsChanged=7
};
inline constexpr bool CameraPassAccepted(OriginalCameraPassResult s) noexcept {
    return s==OriginalCameraPassResult::Initial ||
           s==OriginalCameraPassResult::Continuation;
}
class OriginalProjectedCameraEpoch final {
public:
    void Reset() noexcept { frame_=0;caller_=0;viewport_={};reference_={}; }
    OriginalCameraPassResult Admit(std::uint64_t frame,std::uint32_t caller,
        OriginalProjectionViewport viewport,
        const m2vr::drawprobe::PerspectiveCandidate& fit) noexcept {
        if(!frame || !caller || !viewport.width || !viewport.height ||
           viewport.width>16384u || viewport.height>16384u ||
           viewport.x>16384u || viewport.y>16384u ||
           !fit.plausible || fit.accepted_samples<6u ||
           fit.accepted_samples>24u ||
           !std::isfinite(fit.center_x) || !std::isfinite(fit.center_y) ||
           !std::isfinite(fit.scale_x) || !std::isfinite(fit.scale_y) ||
           !std::isfinite(fit.rms_pixels) ||
           fit.rms_pixels<0.f || fit.rms_pixels>1.25f ||
           std::abs(fit.scale_x)<1.f || std::abs(fit.scale_y)<1.f)
            return OriginalCameraPassResult::Invalid;
        if(!frame_){
            frame_=frame;caller_=caller;viewport_=viewport;reference_=fit;
            return OriginalCameraPassResult::Initial;
        }
        if(frame!=frame_)return OriginalCameraPassResult::FrameChanged;
        if(caller!=caller_)return OriginalCameraPassResult::CallerChanged;
        if(!(viewport==viewport_))return OriginalCameraPassResult::ViewportChanged;
        if(fit.depth_positive!=reference_.depth_positive)
            return OriginalCameraPassResult::DepthChanged;
        // Tolerate bounded least-squares sample noise, but never a
        // shifted projection center or different lens/axis convention.
        const auto near=[](float a,float b,float abs_tol,float rel_tol) {
            return (a>0.f)==(b>0.f) &&
                std::abs(a-b)<=(std::max)(abs_tol,rel_tol*std::abs(b));
        };
        if(std::abs(fit.center_x-reference_.center_x)>
               (std::max)(2.5f,viewport.width*0.005f) ||
           std::abs(fit.center_y-reference_.center_y)>
               (std::max)(2.5f,viewport.height*0.005f) ||
           !near(fit.scale_x,reference_.scale_x,4.f,0.025f) ||
           !near(fit.scale_y,reference_.scale_y,4.f,0.025f))
            return OriginalCameraPassResult::OpticsChanged;
        return OriginalCameraPassResult::Continuation;
    }
private:
    std::uint64_t frame_=0;
    std::uint32_t caller_=0;
    OriginalProjectionViewport viewport_{};
    m2vr::drawprobe::PerspectiveCandidate reference_{};
};
// Internal-only C++ ABI. Calling this without independent original-game
// world proof never authorizes a source frame. A failure is propagated via
// CompleteOriginalD3D9SourceDraw to discard the incomplete CPU eye pair.
bool AdmitVerifiedProjectedD3D9SourcePass(std::uint32_t caller,
    OriginalProjectionViewport viewport,
    const m2vr::drawprobe::PerspectiveCandidate& fit) noexcept;
} // namespace m2vr::vrbridge
