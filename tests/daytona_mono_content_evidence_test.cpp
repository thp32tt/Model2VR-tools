#include "../src/probe/daytona_mono_content_evidence.hpp"
#include "../src/probe/daytona_stereo_color_evidence.hpp"
#include <vector>
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
    // Original real D3D9 GPU eyes may differ only in destination ALPHA;
    // these pixels must not become a counterfeit native OpenXR projection.
    const std::array<std::uint8_t,8> left{
        10u,20u,30u,0u,100u,150u,200u,255u};
    auto right=left;
    assert(!OriginalEyesHaveVisibleRgbDifference(
        left.data(),right.data(),left.size()));
    right[3]=255u;right[7]=0u; // alpha-only mutation
    assert(!OriginalEyesHaveVisibleRgbDifference(
        left.data(),right.data(),left.size()));
    right[6]=201u; // visible B difference in second source pixel
    assert(OriginalEyesHaveVisibleRgbDifference(
        left.data(),right.data(),left.size()));
    right[6]=200u;right[0]=11u;
    assert(OriginalEyesHaveVisibleRgbDifference(
        left.data(),right.data(),left.size()));
    assert(!OriginalEyesHaveVisibleRgbDifference(
        nullptr,right.data(),left.size()));
    assert(!OriginalEyesHaveVisibleRgbDifference(
        left.data(),right.data(),0u));
    assert(!OriginalEyesHaveVisibleRgbDifference(
        left.data(),right.data(),7u));
    std::vector<std::uint8_t> full_left(left.begin(),left.end());
    std::vector<std::uint8_t> full_right(right.begin(),right.end());
    assert(OriginalEyesHaveVisibleRgbDifference(full_left,full_right,8u));
    assert(!OriginalEyesHaveVisibleRgbDifference(full_left,full_right,12u));
    full_right[0]=full_left[0];
    assert(!OriginalEyesHaveVisibleRgbDifference(full_left,full_right,8u));
    return 0;
}
