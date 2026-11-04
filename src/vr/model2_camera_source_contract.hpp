#pragma once
// Camera source/scale is a separately proven, session-stable Model 2 game
// contract. This must never be guessed from a generic D3D9 draw or HMD pose.
#include "model2_cpu_stereo.hpp"
#include <algorithm>
#include <cmath>

namespace m2vr::vr {
class Model2CameraEpochContract final {
public:
    void Reset() noexcept { valid_=false; baseline_={}; }
    bool Anchored() const noexcept {return valid_;}
    // Can be evaluated on a staged copy and committed only after the
    // authenticated live pose and frame latch both succeed.
    bool Admit(const Model2CameraCalibration& candidate) noexcept {
        if(!ValidCalibration(candidate))return false;
        if(!valid_){
            baseline_=candidate;
            valid_=true;
            return true;
        }
        // Axis sign/basis changes without an explicit scene transition can
        // reverse the eye image; changing world meters/near-plane similarly
        // makes parallax jump. Reject, do not silently recenter or rescale.
        for(std::size_t i=0;i<9;++i)
            if(std::abs(baseline_.view_to_xr[i]-
                        candidate.view_to_xr[i])>1.e-4f)return false;
        const auto close_relative=[](float a,float b) noexcept {
            return std::abs(a-b)<=1.e-4f*(std::max)(std::abs(a),std::abs(b));
        };
        return close_relative(baseline_.units_per_meter,
                              candidate.units_per_meter) &&
               close_relative(baseline_.min_depth_m,
                              candidate.min_depth_m);
    }
private:
    bool valid_=false;
    Model2CameraCalibration baseline_{};
};
} // namespace m2vr::vr
