#pragma once
// A Quest controller ray may intersect only the real, stationary desktop
// emulator menu quad. This geometry has no Windows/OpenXR dependencies and
// never grants any original Daytona World3D pass or game input authority.
#include "daytona_mono_quad_anchor.hpp"
#include "model2_emulator_window_policy.hpp"
#include <cmath>
#include <cstdint>
namespace m2vr::menuray {
struct Ray {
    monoquad::Vec3 origin{};
    monoquad::Quat orientation{};
};
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
       window_width>4096u || window_height>4096u)return {};
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
