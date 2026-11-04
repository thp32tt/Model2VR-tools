#include "../src/probe/model2_camera_projection_fit.hpp"
#include <array>
#include <cstdio>
#include <limits>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"camera projection line %d: %s\n",__LINE__,#x);return 1;}}while(0)
using namespace m2vr::drawprobe;
int main(){
    std::array<SourceProjectionSample,6> source{};
    const float x[6]={-2.f,1.f,2.f,-1.f,1.5f,0.f};
    const float y[6]={-1.f,2.f,-2.f,1.5f,0.7f,2.f};
    const float z[6]={5.f,5.f,8.f,8.f,10.f,5.f};
    for(unsigned i=0;i<6;++i){
        source[i].source_xyz={x[i],y[i],z[i]};
        source[i].screen_xy={320.f+400.f*x[i]/z[i],
                             240.f-380.f*y[i]/z[i]};
    }
    const auto camera=FitSourcePerspective(source.data(),6,0,0,640,480);
    CHECK(camera.plausible && camera.accepted_samples==6);
    CHECK(camera.depth_positive && camera.rms_pixels<0.01f);
    CHECK(camera.center_x>319.f && camera.center_x<321.f);
    CHECK(camera.center_y>239.f && camera.center_y<241.f);
    CHECK(camera.scale_x>399.f && camera.scale_x<401.f);
    CHECK(camera.scale_y< -379.f && camera.scale_y> -381.f);
    // Source XY/Z can have a negative depth convention while still being
    // a coherent diagnostic candidate; never silently flip axis calibration.
    for(auto& p:source)p.source_xyz[2]=-p.source_xyz[2];
    const auto negative=FitSourcePerspective(source.data(),6,0,0,640,480);
    CHECK(negative.plausible && !negative.depth_positive);
    source[0].source_xyz[2]=5.f;
    CHECK(!FitSourcePerspective(source.data(),6,0,0,640,480).plausible);
    for(auto& p:source)p.source_xyz[2]=5.f;
    source[1].source_xyz[2]=0.f;
    CHECK(!FitSourcePerspective(source.data(),6,0,0,640,480).plausible);
    source[1].source_xyz[2]=5.f;
    source[0].screen_xy[0]=700.f;
    CHECK(!FitSourcePerspective(source.data(),6,0,0,640,480).plausible);
    source[0].screen_xy[0]=320.f+400.f*x[0]/z[0];
    for(unsigned i=0;i<6;++i)source[i].source_xyz[2]=z[i];
    const float bad=(std::numeric_limits<float>::quiet_NaN)();
    source[2].screen_xy[0]=bad;
    CHECK(!FitSourcePerspective(source.data(),6,0,0,640,480).plausible);
    source[2].screen_xy[0]=320.f+400.f*x[2]/z[2];
    CHECK(!FitSourcePerspective(source.data(),5,0,0,640,480).plausible);
    CHECK(!FitSourcePerspective(source.data(),6,0,0,0,480).plausible);
    CHECK(!FitSourcePerspective(source.data(),6,0,0,640,0).plausible);
    // No x or y depth-dependent spread: insufficient calibration geometry.
    for(auto& p:source){p.source_xyz[0]=0.f;p.source_xyz[1]=0.f;}
    CHECK(!FitSourcePerspective(source.data(),6,0,0,640,480).plausible);
    return 0;
}
