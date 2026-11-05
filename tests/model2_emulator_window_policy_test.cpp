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
    return 0;
}
