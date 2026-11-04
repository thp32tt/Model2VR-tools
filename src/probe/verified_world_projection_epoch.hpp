#pragma once
// Original projected D3D9 source camera continuity across Draws of the SAME
// physical Present. Model matrices may change per object; optical projection
// and viewport cannot switch to a mirror/preview camera under one XR pose.
// Not gameplay-world authentication or calibration.
#include "model2_camera_projection_fit.hpp"
#include <algorithm>
#include <array>
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
           fit.accepted_samples>m2vr::drawprobe::kMaxOriginalProjectionSamples ||
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
        const auto within_tolerance=[](float a,float b,float abs_tol,float rel_tol) {
            return (a>0.f)==(b>0.f) &&
                std::abs(a-b)<=(std::max)(abs_tol,rel_tol*std::abs(b));
        };
        if(std::abs(fit.center_x-reference_.center_x)>
               (std::max)(2.5f,viewport.width*0.005f) ||
           std::abs(fit.center_y-reference_.center_y)>
               (std::max)(2.5f,viewport.height*0.005f) ||
           !within_tolerance(fit.scale_x,reference_.scale_x,4.f,0.025f) ||
           !within_tolerance(fit.scale_y,reference_.scale_y,4.f,0.025f))
            return OriginalCameraPassResult::OpticsChanged;
        return OriginalCameraPassResult::Continuation;
    }
private:
    std::uint64_t frame_=0;
    std::uint32_t caller_=0;
    OriginalProjectionViewport viewport_{};
    m2vr::drawprobe::PerspectiveCandidate reference_{};
};
// Native CPU camera-space triangles, D3D9 XYZ+WVP source and CPU
// ProjectVertexToScreen results are NOT interchangeable source families.
// The first source type latches across all original draws until the same
// Present; a later mixed type fails the whole binocular source transaction.
enum class OriginalCameraSourceFamily : std::uint32_t {
    Native=1u, FixedFunctionD3D9=2u, OriginalCPUProjected=3u
};
class OriginalCameraFamilyEpoch final {
public:
    void Reset() noexcept { frame_=0u;family_=OriginalCameraSourceFamily::Native; }
    bool Admit(std::uint64_t frame,OriginalCameraSourceFamily family) noexcept {
        if(!frame)return false;
        if(!frame_){ frame_=frame;family_=family;return true; }
        return frame==frame_ && family==family_;
    }
private:
    std::uint64_t frame_=0;
    OriginalCameraSourceFamily family_=OriginalCameraSourceFamily::Native;
};
// Preserve only D3D9 VIEW and PROJECTION throughout a frame; WORLD is the
// object's own matrix and is deliberately free to change per original Draw.
// A non-finite, changed axis or changed projection never reaches either eye.
enum class OriginalFixedCameraPassResult : std::uint32_t {
    Initial=0, Continuation=1, Invalid=2, FrameChanged=3,
    ViewportChanged=4, ViewChanged=5, ProjectionChanged=6
};
inline constexpr bool FixedCameraPassAccepted(
    OriginalFixedCameraPassResult value) noexcept {
    return value==OriginalFixedCameraPassResult::Initial ||
           value==OriginalFixedCameraPassResult::Continuation;
}
class OriginalFixedCameraEpoch final {
public:
    void Reset() noexcept {
        frame_=0u;viewport_={};view_={};projection_={};
    }
    OriginalFixedCameraPassResult Admit(
        std::uint64_t frame,OriginalProjectionViewport viewport,
        const std::array<float,16>& view,
        const std::array<float,16>& projection) noexcept {
        if(!frame || !viewport.width || !viewport.height ||
           viewport.width>16384u || viewport.height>16384u ||
           viewport.x>16384u || viewport.y>16384u)
            return OriginalFixedCameraPassResult::Invalid;
        for(float x:view)if(!std::isfinite(x) || std::abs(x)>1.e8f)
            return OriginalFixedCameraPassResult::Invalid;
        for(float x:projection)if(!std::isfinite(x) || std::abs(x)>1.e8f)
            return OriginalFixedCameraPassResult::Invalid;
        if(!frame_){
            frame_=frame;viewport_=viewport;view_=view;projection_=projection;
            return OriginalFixedCameraPassResult::Initial;
        }
        if(frame!=frame_)return OriginalFixedCameraPassResult::FrameChanged;
        if(!(viewport==viewport_))
            return OriginalFixedCameraPassResult::ViewportChanged;
        if(view!=view_)return OriginalFixedCameraPassResult::ViewChanged;
        if(projection!=projection_)
            return OriginalFixedCameraPassResult::ProjectionChanged;
        return OriginalFixedCameraPassResult::Continuation;
    }
private:
    std::uint64_t frame_=0;
    OriginalProjectionViewport viewport_{};
    std::array<float,16> view_{},projection_{};
};
// Internal-only C++ ABI. Calling this without independent original-game
// world proof never authorizes a source frame. A failure is propagated via
// CompleteOriginalD3D9SourceDraw to discard the incomplete CPU eye pair.
bool AdmitVerifiedProjectedD3D9SourcePass(std::uint32_t caller,
    OriginalProjectionViewport viewport,
    const m2vr::drawprobe::PerspectiveCandidate& fit) noexcept;
bool AdmitVerifiedFixedD3D9SourcePass(
    OriginalProjectionViewport viewport,
    const std::array<float,16>& view,
    const std::array<float,16>& projection) noexcept;
} // namespace m2vr::vrbridge
