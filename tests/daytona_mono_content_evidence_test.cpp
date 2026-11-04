#include "../src/probe/daytona_mono_content_evidence.hpp"
#include <array>
#include <cassert>
using namespace m2vr::daytona;
int main(){
    constexpr std::uint32_t w=32,h=18,pitch=w*4;
    std::array<std::uint8_t,pitch*h> frame{};
    auto e=InspectOriginalMonoRGBA(frame.data(),frame.size(),w,h,pitch);
    assert(e.sampled==144 && e.nonblack==0 && e.varied==0);
    for(std::size_t i=0;i<frame.size();i+=4u)frame[i]=50u;
    e=InspectOriginalMonoRGBA(frame.data(),frame.size(),w,h,pitch);
    assert(e.sampled==144 && e.nonblack==144 && e.varied==0);
    for(std::uint32_t y=h/2;y<h;++y)
        for(std::uint32_t x=0;x<w;++x)frame[(static_cast<std::size_t>(y)*w+x)*4u+1]=220u;
    e=InspectOriginalMonoRGBA(frame.data(),frame.size(),w,h,pitch);
    assert(e.sampled==144 && e.nonblack==144 && e.varied>0 && e.varied<144);
    assert(InspectOriginalMonoRGBA(nullptr,0,w,h,pitch).sampled==0);
    assert(InspectOriginalMonoRGBA(frame.data(),1,w,h,pitch).sampled==0);
    assert(InspectOriginalMonoRGBA(frame.data(),frame.size(),w,h,2).sampled==0);
    return 0;
}
