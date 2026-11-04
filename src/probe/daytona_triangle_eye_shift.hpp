#pragma once
// Verified shader-UP source triangle eye shift. Original (untrusted) bytes
// must already have been copied into two independently owned bounded buffers.
// Never shift part of a triangle across a W-sign/near-plane discontinuity.
#include "daytona_eye_pose_gate.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace m2vr::daytona {
struct ShiftResult {
    std::uint32_t shifted=0u;
    std::uint32_t held_flat=0u;
};
inline ShiftResult ShiftSafeUPTriangles(
    std::uint8_t* left_eye,std::uint8_t* right_eye,
    std::size_t bytes,std::uint32_t triangles,float eye_offset)noexcept {
    if(!left_eye||!right_eye||left_eye==right_eye||!triangles||
       triangles>512u || bytes!=std::size_t{triangles}*96u ||
       !std::isfinite(eye_offset) || eye_offset<=0.f)return {};
    ShiftResult result{};
    for(std::uint32_t tri=0;tri<triangles;++tri) {
        std::array<float,3> l{},r{};
        bool safe=true;
        const std::size_t base=std::size_t{tri}*96u;
        // Check all 3 source vertices BEFORE mutating any triangle bytes.
        for(std::size_t v=0;v<3u;++v) {
            const auto offset=base+v*32u;
            float x=0.f,y=0.f,z=0.f,w=0.f;
            std::memcpy(&x,left_eye+offset,sizeof x);
            std::memcpy(&y,left_eye+offset+4u,sizeof y);
            std::memcpy(&z,left_eye+offset+8u,sizeof z);
            std::memcpy(&w,left_eye+offset+12u,sizeof w);
            if(!std::isfinite(x) || !std::isfinite(y) ||
               !std::isfinite(z) || !SafeStereoHomogeneousW(w)){
                safe=false;break;
            }
            const float left=x+eye_offset,right=x-eye_offset;
            if(!std::isfinite(left)||!std::isfinite(right)){
                safe=false;break;
            }
            l[v]=left;r[v]=right;
        }
        if(!safe){++result.held_flat;continue;}
        for(std::size_t v=0;v<3u;++v){
            const auto offset=base+v*32u;
            std::memcpy(left_eye+offset,&l[v],sizeof(float));
            std::memcpy(right_eye+offset,&r[v],sizeof(float));
        }
        ++result.shifted;
    }
    return result;
}
} // namespace m2vr::daytona
