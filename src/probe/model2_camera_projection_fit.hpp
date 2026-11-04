#pragma once
// Original CPU ProjectVertexToScreen -> original D3D9 triangle projection
// consistency estimator. This is a bounded and read-only camera HYPOTHESIS,
// not proof of a gameplay world pass, game units-per-meter or VR calibration.
// No game memory or proprietary assets are stored.
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace m2vr::drawprobe {
// Source matches are capped at 64 original triangles (192 points),
// strictly below the 256-point same-thread CPU projector witness ring.
inline constexpr std::size_t kMaxOriginalProjectionSamples=192u;
struct SourceProjectionSample {
    std::array<float,3> source_xyz{};
    std::array<float,2> screen_xy{};
};
struct PerspectiveCandidate {
    std::uint32_t accepted_samples=0;
    float center_x=0,center_y=0,scale_x=0,scale_y=0;
    float rms_pixels=0;
    bool depth_positive=false;
    bool plausible=false;
};
// Least-squares screen projection against independently sampled CPU-space
// original vertex coordinates: px=cx+fx*x/abs(z), py=cy+fy*y/abs(z).
// The sign of fx/fy and z is retained for private camera-axis investigation.
// A low residual is NOT an authenticated world, HUD or main-camera label.
inline PerspectiveCandidate FitSourcePerspective(
    const SourceProjectionSample* points,std::size_t count,
    std::uint32_t viewport_x,std::uint32_t viewport_y,
    std::uint32_t viewport_width,std::uint32_t viewport_height) noexcept {
    PerspectiveCandidate out{};
    if(!points || count<6u || count>kMaxOriginalProjectionSamples ||
       !viewport_width || !viewport_height ||
       viewport_width>16384u || viewport_height>16384u ||
       viewport_x>16384u || viewport_y>16384u)return out;
    double mx=0,my=0,mpx=0,mpy=0;
    int depth_sign=0;
    std::array<std::array<double,4>,kMaxOriginalProjectionSamples> samples{};
    for(std::size_t i=0;i<count;++i) {
        const auto& p=points[i];
        for(float v:p.source_xyz)
            if(!std::isfinite(v) || std::abs(v)>1.e7f)return out;
        for(float v:p.screen_xy)
            if(!std::isfinite(v))return out;
        const double z=p.source_xyz[2];
        if(std::abs(z)<0.0001)return out;
        const int sign=z>0?1:-1;
        if(depth_sign && depth_sign!=sign)return out;
        depth_sign=sign;
        const double xx=p.source_xyz[0]/std::abs(z);
        const double yy=p.source_xyz[1]/std::abs(z);
        const double px=p.screen_xy[0],py=p.screen_xy[1];
        if(!std::isfinite(xx) || !std::isfinite(yy) ||
           std::abs(xx)>1000 || std::abs(yy)>1000 ||
           px<static_cast<double>(viewport_x)-viewport_width*0.25 ||
           py<static_cast<double>(viewport_y)-viewport_height*0.25 ||
           px>static_cast<double>(viewport_x)+viewport_width*1.25 ||
           py>static_cast<double>(viewport_y)+viewport_height*1.25)
            return out;
        samples[i]={xx,yy,px,py};
        mx+=xx;my+=yy;mpx+=px;mpy+=py;
    }
    const double n=static_cast<double>(count);
    mx/=n;my/=n;mpx/=n;mpy/=n;
    double vx=0,vy=0,covx=0,covy=0;
    for(std::size_t i=0;i<count;++i) {
        const auto& v=samples[i];
        vx+=(v[0]-mx)*(v[0]-mx);
        vy+=(v[1]-my)*(v[1]-my);
        covx+=(v[0]-mx)*(v[2]-mpx);
        covy+=(v[1]-my)*(v[3]-mpy);
    }
    if(vx<1.e-5 || vy<1.e-5)return out;
    const double fx=covx/vx,fy=covy/vy;
    const double cx=mpx-fx*mx,cy=mpy-fy*my;
    if(!std::isfinite(fx) || !std::isfinite(fy) ||
       !std::isfinite(cx) || !std::isfinite(cy) ||
       std::abs(fx)<1.0 || std::abs(fy)<1.0 ||
       std::abs(fx)>1.e6 || std::abs(fy)>1.e6 ||
       cx<static_cast<double>(viewport_x)-viewport_width ||
       cy<static_cast<double>(viewport_y)-viewport_height ||
       cx>static_cast<double>(viewport_x)+2*viewport_width ||
       cy>static_cast<double>(viewport_y)+2*viewport_height)return out;
    double error=0;
    for(std::size_t i=0;i<count;++i){
        const auto& v=samples[i];
        const double dx=v[2]-(cx+fx*v[0]);
        const double dy=v[3]-(cy+fy*v[1]);
        const double point_error=dx*dx+dy*dy;
        // A single incorrectly joined HUD/preview point must not become
        // admissible just because a large 192-point batch dilutes RMS.
        if(!std::isfinite(point_error) || point_error>6.25)return out;
        error+=point_error;
    }
    const double rms=std::sqrt(error/n);
    if(!std::isfinite(rms) || rms>1.25)return out;
    out.accepted_samples=static_cast<std::uint32_t>(count);
    out.center_x=static_cast<float>(cx);
    out.center_y=static_cast<float>(cy);
    out.scale_x=static_cast<float>(fx);
    out.scale_y=static_cast<float>(fy);
    out.rms_pixels=static_cast<float>(rms);
    out.depth_positive=depth_sign>0;
    out.plausible=true;
    return out;
}
} // namespace m2vr::drawprobe
