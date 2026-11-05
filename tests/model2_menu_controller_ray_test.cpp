#include "../src/common/model2_menu_controller_ray.hpp"
#include <cassert>
#include <cmath>
#include <limits>
using namespace m2vr;
int main(){
    monoquad::Placement screen{};
    screen.position={0.f,0.f,-2.f};
    screen.orientation={0.f,0.f,0.f,1.f};
    screen.width=2.f;screen.height=2.f;
    const menuray::Ray forward{{0.f,0.f,0.f},{0.f,0.f,0.f,1.f}};
    const auto center=menuray::Intersect(forward,screen,640,480,1024,1024);
    assert(center.valid);
    assert(center.texture_x==512u && center.texture_y==512u);
    assert(center.window_x==320u && center.window_y==240u);
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
