#pragma once
// Rebase a proved CPU-world camera from absolute OpenXR LOCAL pose to the
// headset's first accepted orientation and stereo-eye midpoint. This cannot
// establish game/world proof or calibrate the original emulator camera.
#include "model2_cpu_stereo.hpp"
#include "../common/stereo_eye_rig_geometry.hpp"
#include <array>
#include <cmath>
#include <cstdint>

namespace m2vr::vr {
class Model2HeadRecenter final {
public:
    void Reset() noexcept {
        anchored_=false;
        center_={};
        orientation_={0.f,0.f,0.f,1.f};
        reference_space_epoch_=0u;
    }
    bool Anchored() const noexcept { return anchored_; }
    std::uint32_t ReferenceSpaceEpoch()const noexcept{return reference_space_epoch_;}

    // Atomic: an invalid packet cannot change the current origin or output.
    // Callers must also check tracking age and game-frame sequence.
    bool Rebase(const pose::Frame& input,pose::Frame& output) noexcept {
        // A LOCAL recenter changes the meaning of every absolute eye
        // position. Reject older/legacy epochs after a new one is latched.
        // Do not mutate the old origin until the whole rebase succeeds.
        // A genuine original CPU-world origin requires explicit LOCAL
        // provenance even on the very first accepted headset frame.
        if(!input.reserved ||
           (reference_space_epoch_ &&
            input.reserved<reference_space_epoch_))
            return false;
        // A valid-but-inferred headset pose must never reanchor the
        // original game's CPU-world camera as genuine tracked 6DoF.
        if((input.view_flags&pose::kFullSixDofTracked)!=
           pose::kFullSixDofTracked)return false;
        if(!pose::CheckStereoRigGeometry(input).valid())return false;
        for(const auto& eye:input.eyes){
            if(!QuaternionValid(eye.orientation_xyzw))return false;
            for(const float p:eye.position_m)
                if(!std::isfinite(p) || std::abs(p)>10000.f)return false;
            const auto& f=eye.fov_left_right_up_down;
            for(const float a:f)
                if(!std::isfinite(a)||a<=-1.55f||a>=1.55f)return false;
            if(!(f[0]<0.f && f[1]>0.f && f[2]>0.f && f[3]<0.f))
                return false;
        }
        const auto& left=input.eyes[0];
        const auto& right=input.eyes[1];
        double squared_ipd=0.;
        for(int i=0;i<3;++i){
            const double d=double(right.position_m[i])-left.position_m[i];
            squared_ipd+=d*d;
        }
        if(squared_ipd<0.02*0.02 || squared_ipd>0.11*0.11)return false;
        const bool changed_epoch=input.reserved!=reference_space_epoch_;
        const bool already_anchored=anchored_ && !changed_epoch;
        auto center=changed_epoch ? std::array<float,3>{} : center_;
        auto orientation=changed_epoch ?
            std::array<float,4>{0.f,0.f,0.f,1.f} : orientation_;
        if(!already_anchored){
            for(int i=0;i<3;++i)
                center[i]=static_cast<float>(
                    (double(left.position_m[i])+right.position_m[i])*0.5);
            // Canted OpenXR eyes can have distinct, equally valid view
            // orientations. The left eye alone is NOT the headset's
            // central forward direction: anchoring to it produces a
            // persistent asymmetric yaw bias in head-relative World3D.
            // q and -q encode the same rotation, so align hemispheres
            // before averaging the two normalized, rig-checked views.
            double agreement=0.;
            for(int i=0;i<4;++i)
                agreement+=double(left.orientation_xyzw[i])*
                           double(right.orientation_xyzw[i]);
            const float right_sign=agreement<0. ? -1.f : 1.f;
            for(int i=0;i<4;++i)
                orientation[i]=0.5f*(left.orientation_xyzw[i]+
                                    right_sign*right.orientation_xyzw[i]);
            Normalize(orientation);
            if(!QuaternionValid(orientation.data()))return false;
        }
        pose::Frame candidate=input;
        for(int eye_index=0;eye_index<2;++eye_index){
            auto& eye=candidate.eyes[eye_index];
            const auto& source=input.eyes[eye_index];
            const CpuPoint relative{
                source.position_m[0]-center[0],
                source.position_m[1]-center[1],
                source.position_m[2]-center[2]};
            const CpuPoint local=RotateInverse(orientation.data(),relative);
            if(!Finite(local.x)||!Finite(local.y)||!Finite(local.z))
                return false;
            eye.position_m[0]=local.x;
            eye.position_m[1]=local.y;
            eye.position_m[2]=local.z;
            std::array<float,4> actual{
                source.orientation_xyzw[0],source.orientation_xyzw[1],
                source.orientation_xyzw[2],source.orientation_xyzw[3]};
            Normalize(actual);
            // OpenXR may switch between q and -q for the SAME physical
            // eye rotation from frame to frame or after LOCAL recenter.
            // Canonicalize each eye to the captured binocular-midpoint
            // hemisphere so downstream per-eye interpolation sees no
            // artificial full-turn discontinuity.
            double agreement=0.;
            for(int i=0;i<4;++i)
                agreement+=double(actual[i])*orientation[i];
            if(agreement<0.)
                for(float& value:actual)value=-value;
            const float x=-orientation[0],y=-orientation[1],
                        z=-orientation[2],w=orientation[3];
            const float a=actual[0],b=actual[1],
                        c=actual[2],d=actual[3];
            std::array<float,4> local_q{
                w*a+x*d+y*c-z*b,
                w*b-x*c+y*d+z*a,
                w*c+x*b-y*a+z*d,
                w*d-x*a-y*b-z*c};
            Normalize(local_q);
            if(!QuaternionValid(local_q.data()))return false;
            for(int i=0;i<4;++i)eye.orientation_xyzw[i]=local_q[i];
        }
        output=candidate;
        if(!already_anchored){
            center_=center;
            orientation_=orientation;
            anchored_=true;
        }
        reference_space_epoch_=input.reserved;
        return true;
    }
private:
    static void Normalize(std::array<float,4>& q) noexcept {
        double squared=0.;
        for(const float v:q)squared+=double(v)*v;
        const float inv=static_cast<float>(1./std::sqrt(squared));
        for(auto& v:q)v*=inv;
    }
    std::uint32_t reference_space_epoch_=0u;
    bool anchored_=false;
    std::array<float,3> center_{};
    std::array<float,4> orientation_{0.f,0.f,0.f,1.f};
};
} // namespace m2vr::vr
