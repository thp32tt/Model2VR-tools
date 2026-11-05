#include "../src/common/model2_emulator_window_policy.hpp"
#include <cassert>
using namespace m2vr::desktopmenu;
int main(){
    // Dual-monitor Windows may place its primary origin to the RIGHT of
    // a second monitor. Quest rays/real cursor use signed VIRTUAL origin.
    static_assert(DesktopBounds{-1920,-240,5360,1680}.Valid());
    static_assert(!DesktopBounds{0,0,9000,1440}.Valid());
    constexpr auto desktop=DesktopBounds{-1920,-240,5360,1680};
    constexpr auto at_origin=MapDesktopPoint(desktop,1920,240);
    static_assert(at_origin.valid && at_origin.x==0 && at_origin.y==0);
    constexpr auto at_secondary=MapDesktopPoint(desktop,0,0);
    static_assert(at_secondary.valid &&
                  at_secondary.x==-1920 && at_secondary.y==-240);
    static_assert(!MapDesktopPoint(desktop,5360,100).valid);
    static_assert(!MapDesktopPoint(desktop,100,1680).valid);
    static_assert(!MapDesktopPoint({},100,100).valid);
    static_assert(Fit(5360,1680,1024,1024).Valid());
    // Real mouse still has a valid texture position OUTSIDE the small
    // emulator window, including on the negative-X second monitor.
    constexpr auto wide_fit=Fit(5360,1680,1024,1024);
    constexpr auto desktop_cursor=PlacePointer(-1500,300,
        desktop.left,desktop.top,desktop.width,desktop.height,
        wide_fit,32,32,1,1);
    static_assert(desktop_cursor.Valid());
    static_assert(!Fit(0,480,1024,1024).Valid());
    static_assert(!Fit(640,0,1024,1024).Valid());
    static_assert(!Fit(640,480,0,1024).Valid());
    static_assert(Fit(640,480,1024,1024).x==0);
    static_assert(Fit(640,480,1024,1024).y==128);
    static_assert(Fit(640,480,1024,1024).height==768);
    static_assert(Fit(1920,1080,1024,1024).height==576);
    static_assert(Fit(1920,1080,1024,1024).y==224);
    static_assert(Fit(800,1200,1024,1024).width==682);
    static_assert(Fit(800,1200,1024,1024).x==171);
    static_assert(!Fit(9000,1200,1024,1024).Valid());
    static_assert(Choose(false,false,true)==Source::EmulatorWindow2D);
    static_assert(Choose(false,true,true)==Source::EmulatorWindow2D);
    static_assert(Choose(true,false,true)==Source::GameMono2D);
    static_assert(Choose(true,true,true)==Source::VerifiedGameStereo);
    static_assert(Choose(false,false,false)==Source::Diagnostic);
    static_assert(MenuUploadToken(0)==0);
    static_assert(MenuUploadToken(1)==(std::uint64_t{1}<<63 | 1u));
    assert(MenuUploadToken(101)!=MenuUploadToken(102));
    constexpr auto fit=Fit(640,480,1024,1024);
    constexpr auto pointer=PlacePointer(420,290,100,50,640,480,
                                       fit,32,32,5,5);
    static_assert(pointer.Valid());
    static_assert(pointer.x==504 && pointer.y==504);
    static_assert(pointer.width==51 && pointer.height==51);
    static_assert(!PlacePointer(99,290,100,50,640,480,
                               fit,32,32,0,0).Valid());
    static_assert(!PlacePointer(420,530,100,50,640,480,
                               fit,32,32,0,0).Valid());
    static_assert(!PlacePointer(420,290,100,50,640,480,
                               fit,32,32,32,0).Valid());
    return 0;
}
