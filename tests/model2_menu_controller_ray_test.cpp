#include "../src/common/model2_menu_controller_ray.hpp"
#include <cassert>
#include <cmath>
#include <limits>
using namespace m2vr;
int main(){
    // OpenXR XrTime is signed nanoseconds. Exactly 150ms is accepted;
    // stale/reordered/unknown presses must not inject a desktop click.
    static_assert(menuray::FreshPressTime(1000000000LL,1000000000LL));
    static_assert(menuray::FreshPressTime(1000000000LL,850000000LL));
    static_assert(!menuray::FreshPressTime(1000000000LL,849999999LL));
    static_assert(!menuray::FreshPressTime(1000000000LL,1000000001LL));
    static_assert(!menuray::FreshPressTime(1000000000LL,0));
    static_assert(!menuray::FreshPressTime(0,0));
    static_assert(!menuray::FreshPressTime(-1,1));
    static_assert(!menuray::FreshPressTime(1,-1));
    static_assert(!menuray::FreshPressTime(
        std::numeric_limits<std::int64_t>::max(),1));
    static_assert(menuray::FreshPressTime(
        std::numeric_limits<std::int64_t>::max(),
        std::numeric_limits<std::int64_t>::max()-150000000));
    static_assert(menuray::PickHand(false,false,true,true)==-1);
    static_assert(menuray::PickHand(true,false,true,false)==0);
    static_assert(menuray::PickHand(false,true,false,true)==1);
    static_assert(menuray::PickHand(true,true,true,false)==0);
    static_assert(menuray::PickHand(true,true,false,true)==1);
    static_assert(menuray::PickHand(true,true,true,true)==1);
    static_assert(menuray::PickHand(true,true,false,false)==1);
    // Hover follows a live predicted pose, while click stays at the
    // separately tracked historical OpenXR trigger-edge pose.
    constexpr auto moved_left=menuray::SelectMenuTargets(
        false,true,true,false);
    static_assert(moved_left.hover_hand==1 && moved_left.click_hand==0);
    constexpr auto left_still_hovering=menuray::SelectMenuTargets(
        true,true,true,false);
    static_assert(left_still_hovering.hover_hand==0 &&
                  left_still_hovering.click_hand==0);
    constexpr auto untracked_press=menuray::SelectMenuTargets(
        false,true,false,false);
    static_assert(untracked_press.hover_hand==1 &&
                  untracked_press.click_hand==-1);
    constexpr auto moved_right=menuray::SelectMenuTargets(
        false,false,false,true);
    static_assert(moved_right.hover_hand==-1 && moved_right.click_hand==1);
    constexpr auto both_press=menuray::SelectMenuTargets(
        true,true,true,true);
    static_assert(both_press.hover_hand==1 && both_press.click_hand==1);
    constexpr auto no_target=menuray::SelectMenuTargets(
        false,false,false,false);
    static_assert(no_target.hover_hand==-1 && no_target.click_hand==-1);
    monoquad::Placement screen{};
    screen.position={0.f,0.f,-2.f};
    screen.orientation={0.f,0.f,0.f,1.f};
    screen.width=2.f;screen.height=2.f;
    const menuray::Ray forward{{0.f,0.f,0.f},{0.f,0.f,0.f,1.f}};
    const auto center=menuray::Intersect(forward,screen,640,480,1024,1024);
    assert(center.valid);
    assert(center.texture_x==512u && center.texture_y==512u);
    assert(center.window_x==320u && center.window_y==240u);
    // A full 5760px virtual desktop must remain ray-addressable,
    // with the exact click mapping back to its signed screen origin.
    const auto full_desktop=menuray::Intersect(
        forward,screen,5760,1440,1024,1024);
    assert(full_desktop.valid);
    assert(full_desktop.window_x==2880u && full_desktop.window_y==720u);
    constexpr desktopmenu::DesktopBounds physical{-1920,-120,5760,1440};
    const auto click=desktopmenu::MapDesktopPoint(
        physical,full_desktop.window_x,full_desktop.window_y);
    assert(click.valid && click.x==960 && click.y==600);
    assert(!menuray::Intersect(
        forward,screen,8193,1440,1024,1024).valid);
    const auto right=menuray::Intersect(
        {{0.5f,0.f,0.f},{0.f,0.f,0.f,1.f}},screen,640,480,1024,1024);
    assert(right.valid && right.texture_x==768u && right.window_x==480u);
    // A ray can hit the physical square without hitting letterboxed pixels.
    assert(!menuray::Intersect(
        {{0.f,0.9f,0.f},{0.f,0.f,0.f,1.f}},screen,640,480,1024,1024).valid);
    assert(!menuray::Intersect(
        {{2.1f,0.f,0.f},{0.f,0.f,0.f,1.f}},screen,640,480,1024,1024).valid);
    // A reversed controller (aiming away from the virtual desktop) is inert.
    assert(!menuray::Intersect(
        {{0.f,0.f,0.f},{0.f,1.f,0.f,0.f}},screen,640,480,1024,1024).valid);
    assert(!menuray::Intersect(
        {{0.f,0.f,-4.f},{0.f,0.f,0.f,1.f}},screen,640,480,1024,1024).valid);
    assert(!menuray::Intersect(
        {{0.f,0.f,0.f},{0.f,0.f,0.f,0.f}},screen,640,480,1024,1024).valid);
    assert(!menuray::Intersect(forward,screen,0,480,1024,1024).valid);
    auto bad=screen;
    bad.position.x=std::numeric_limits<float>::quiet_NaN();
    assert(!menuray::Intersect(forward,bad,640,480,1024,1024).valid);
    // Yaw-rotated desk: local forward and controller ray must rotate together.
    constexpr float s=0.70710678f;
    screen.position={-2.f,0.f,0.f};
    screen.orientation={0.f,s,0.f,s};
    const auto sideways=menuray::Intersect(
        {{0.f,0.f,0.f},{0.f,s,0.f,s}},screen,640,480,1024,1024);
    assert(sideways.valid && sideways.window_x==320u && sideways.window_y==240u);
    return 0;
}
