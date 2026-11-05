#include "../src/common/model2_emulator_window_policy.hpp"
#include <cassert>
using namespace m2vr::desktopmenu;
int main(){
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
