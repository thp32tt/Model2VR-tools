#pragma once
// A Quest controller ray may intersect only the real, stationary desktop
// emulator menu quad. This geometry has no Windows/OpenXR dependencies and
// never grants any original Daytona World3D pass or game input authority.
#include "daytona_mono_quad_anchor.hpp"
#include "model2_emulator_window_policy.hpp"
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>
namespace m2vr::menuray {
struct Ray {
    monoquad::Vec3 origin{};
    monoquad::Quat orientation{};
};
// Match each trigger to ITS OWN tracked ray. A left-trigger selection must
// never be stolen by an idle right-hand hover (or vice versa). Right hand
// wins ties only if both hands actually click in the same XR sync.
// A boolean trigger edge occurred at XrActionStateBoolean::lastChangeTime.
// Reject zero, future and stale action timestamps instead of aiming a delayed
// click with the *predicted display* pose at a different menu button.
// XrTime is signed nanoseconds; ordered positive values make subtraction safe.
constexpr bool FreshPressTime(std::int64_t predicted_ns,
                              std::int64_t changed_ns) noexcept {
    constexpr std::int64_t kMaxPressLagNs=150000000; // 150ms XR sync lease
    return predicted_ns>0 && changed_ns>0 &&
           predicted_ns>=changed_ns &&
           predicted_ns-changed_ns<=kMaxPressLagNs;
}
// An OpenXR runtime may return an old isActive/changedSinceLastSync edge
// during a temporary action resync. One already observed lastChangeTime
// must never generate a SECOND Windows mouse click. Store the accepted
// timestamp for each controller independently for the entire XR session,
// including clicks whose target is rejected by the existing HWND gates.
// Only the real XR_SESSION_STATE_READY -> xrBeginSession transition resets
// it, because an OpenXR runtime may change XrTime's session baseline.
class PressEventGate final {
public:
    constexpr void BeginSession() noexcept {last_event_ns_={0,0};}
    constexpr bool Consume(std::size_t hand,std::int64_t event_ns) noexcept {
        if(hand>=last_event_ns_.size() || event_ns<=0 ||
           event_ns<=last_event_ns_[hand])return false;
        last_event_ns_[hand]=event_ns;
        return true;
    }
private:
    std::array<std::int64_t,2> last_event_ns_{};
};
// Menu activation is a RELEASE-TO-ARM gesture, not merely a fresh event.
// A held trigger present when a menu appears or after tracking/focus loss
// must not click a newly exposed desktop button. Both hands must observe
// their own active, tracked UP state on the actual visible menu before an
// edge can cause another Win32 click. A valid press consumes its arm even
// if its pixel/HWND/submitted-screenshot checks later block the click.
// This is separate from the per-session monotonically increasing event
// timestamp gate; no new World3D/game input authority is created.
class MenuClickArmGate final {
public:
    constexpr void Reset() noexcept {released_on_menu_={false,false};}
    constexpr bool Observe(std::size_t hand,bool pose_active,
                           bool action_active,bool pressed,
                           bool press_edge) noexcept {
        if(hand>=released_on_menu_.size())return false;
        if(!pose_active || !action_active){
            released_on_menu_[hand]=false;
            return false;
        }
        if(!pressed){
            released_on_menu_[hand]=true;
            return false;
        }
        if(!press_edge || !released_on_menu_[hand])return false;
        released_on_menu_[hand]=false;
        return true;
    }
private:
    std::array<bool,2> released_on_menu_{};
};
constexpr int PickHand(bool left_hit,bool right_hit,
                       bool left_click,bool right_click) noexcept {
    if(right_hit && right_click)return 1;
    if(left_hit && left_click)return 0;
    if(right_hit)return 1;
    return left_hit?0:-1;
}
// Visual hover is a predicted-display-time sample; button activation uses
// the ACTUAL OpenXR press-time sample. A moving hand must not rewind the
// reticle to its historical trigger pose or steal a click from another hand.
struct MenuTargets {
    int hover_hand=-1;
    int click_hand=-1;
};
constexpr MenuTargets SelectMenuTargets(
    bool left_hover,bool right_hover,
    bool left_press_hit,bool right_press_hit) noexcept {
    const int click=PickHand(left_press_hit,right_press_hit,
                             left_press_hit,right_press_hit);
    // When the clicked hand is still hovering the quad, display that live
    // reticle; otherwise keep the other hand's current hover, if any.
    const int hover=PickHand(left_hover,right_hover,
                             click==0 && left_hover,
                             click==1 && right_hover);
    return {hover,click};
}
struct Hit {
    bool valid=false;
    std::uint32_t texture_x=0,texture_y=0;
    std::uint32_t window_x=0,window_y=0;
};
inline float Dot(monoquad::Vec3 a,monoquad::Vec3 b) noexcept {
    return a.x*b.x+a.y*b.y+a.z*b.z;
}
inline Hit Intersect(const Ray& ray,const monoquad::Placement& quad,
                     std::uint32_t window_width,std::uint32_t window_height,
                     std::uint32_t tex_width,std::uint32_t tex_height) noexcept {
    if(!monoquad::Finite(ray.origin) || !monoquad::Finite(quad.position) ||
       !std::isfinite(quad.width) || !std::isfinite(quad.height) ||
       quad.width<0.5f || quad.width>4.f ||
       quad.height<0.5f || quad.height>4.f ||
       window_width<160u || window_height<100u ||
       window_width>8192u || window_height>8192u)return {};
    const auto fit=desktopmenu::Fit(window_width,window_height,tex_width,tex_height);
    if(!fit.Valid())return {};
    auto ray_q=ray.orientation,quad_q=quad.orientation;
    if(!monoquad::Normalize(ray_q) || !monoquad::Normalize(quad_q))return {};
    const auto direction=monoquad::Rotate(ray_q,{0.f,0.f,-1.f});
    const auto normal=monoquad::Rotate(quad_q,{0.f,0.f,1.f});
    const float denominator=Dot(direction,normal);
    // Only the front face can be clicked; near-parallel or backwards rays
    // must never become desktop input.
    if(!std::isfinite(denominator) || denominator>=-0.05f)return {};
    const monoquad::Vec3 to_center{quad.position.x-ray.origin.x,
        quad.position.y-ray.origin.y,quad.position.z-ray.origin.z};
    const float distance=Dot(to_center,normal)/denominator;
    if(!std::isfinite(distance) || distance<0.05f || distance>10.f)return {};
    const monoquad::Vec3 hit_world{
        ray.origin.x+direction.x*distance-quad.position.x,
        ray.origin.y+direction.y*distance-quad.position.y,
        ray.origin.z+direction.z*distance-quad.position.z
    };
    if(!monoquad::Finite(hit_world))return {};
    const monoquad::Quat inverse{-quad_q.x,-quad_q.y,-quad_q.z,quad_q.w};
    const auto local=monoquad::Rotate(inverse,hit_world);
    const float u=local.x/quad.width+0.5f;
    const float v=0.5f-local.y/quad.height;
    if(!std::isfinite(u) || !std::isfinite(v) ||
       u<0.f || u>=1.f || v<0.f || v>=1.f)return {};
    const auto tx=static_cast<std::uint32_t>(u*tex_width);
    const auto ty=static_cast<std::uint32_t>(v*tex_height);
    // Exclude black letterboxing. Never click an unrelated window through
    // the unused area of the square XR swapchain.
    if(tx<fit.x || ty<fit.y ||
       tx>=fit.x+fit.width || ty>=fit.y+fit.height)return {};
    const auto x=static_cast<std::uint32_t>(
        (std::uint64_t(tx-fit.x)*window_width)/fit.width);
    const auto y=static_cast<std::uint32_t>(
        (std::uint64_t(ty-fit.y)*window_height)/fit.height);
    if(x>=window_width || y>=window_height)return {};
    return {true,tx,ty,x,y};
}
} // namespace m2vr::menuray
