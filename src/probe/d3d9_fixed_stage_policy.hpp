#pragma once
// Exact fixed-function D3D9 stage-zero TEXTURE/DIFFUSE operation contract.
// Unknown operations (TFACTOR, COMPLEMENT, ADD, DOT3, disabled source, etc.)
// are not approximated inside a calibrated native stereo world pass.
#include <d3d9.h>
#include <cstdint>

namespace m2vr::drawprobe {
// Stage-zero original fixed-function RGB and ALPHA are independent.
// The three modes require actual source texture and/or D3DCOLOR data.
// They are not a gameplay-world classification or render permission.
enum class FixedStageSource : std::uint32_t {
    Texture=0u, Diffuse=1u, Modulate=2u
};
inline constexpr bool ValidFixedStageSource(FixedStageSource s) noexcept {
    return s==FixedStageSource::Texture ||
           s==FixedStageSource::Diffuse ||
           s==FixedStageSource::Modulate;
}
inline constexpr bool FixedStageUsesTexture(FixedStageSource s) noexcept {
    return s==FixedStageSource::Texture || s==FixedStageSource::Modulate;
}
inline constexpr bool FixedStageUsesDiffuse(FixedStageSource s) noexcept {
    return s==FixedStageSource::Diffuse || s==FixedStageSource::Modulate;
}
inline constexpr bool FixedStagePairIsDiffuseOnly(
    FixedStageSource rgb,FixedStageSource alpha) noexcept {
    return rgb==FixedStageSource::Diffuse && alpha==FixedStageSource::Diffuse;
}
inline constexpr bool FixedStagePairUsesTexture(
    FixedStageSource rgb,FixedStageSource alpha) noexcept {
    return FixedStageUsesTexture(rgb) || FixedStageUsesTexture(alpha);
}
inline bool DecodeFixedStageSource(DWORD operation,DWORD first,
                                   DWORD second,FixedStageSource& out) noexcept {
    FixedStageSource next{};
    if(operation==D3DTOP_SELECTARG1 || operation==D3DTOP_SELECTARG2){
        // The unused argument MUST NOT influence the selected channel.
        const DWORD selected=operation==D3DTOP_SELECTARG1?first:second;
        if(selected==D3DTA_TEXTURE)next=FixedStageSource::Texture;
        else if(selected==D3DTA_DIFFUSE || selected==D3DTA_CURRENT)
            next=FixedStageSource::Diffuse; // exact D3D9 stage0 equivalence
        else return false; // CURRENT, TFACTOR, flags and unknown source
    }else if(operation==D3DTOP_MODULATE &&
       ((first==D3DTA_TEXTURE &&
         (second==D3DTA_DIFFUSE || second==D3DTA_CURRENT)) ||
        ((first==D3DTA_DIFFUSE || first==D3DTA_CURRENT) &&
         second==D3DTA_TEXTURE))){
        next=FixedStageSource::Modulate;
    }else return false;
    out=next;
    return true;
}

inline bool DecodeFixedTextureDiffuseOp(
    DWORD operation,DWORD first,DWORD second,bool& modulation) noexcept {
    if((operation==D3DTOP_SELECTARG1 && first==D3DTA_TEXTURE) ||
       (operation==D3DTOP_SELECTARG2 && second==D3DTA_TEXTURE)){
        // Both original argument selectors reproduce the same texel;
        // unused argument state does not modify the selected color.
        modulation=false;
        return true;
    }
    if(operation==D3DTOP_MODULATE &&
       ((first==D3DTA_TEXTURE && second==D3DTA_DIFFUSE) ||
        (first==D3DTA_DIFFUSE && second==D3DTA_TEXTURE))){
        modulation=true;
        return true;
    }
    return false;
}
} // namespace m2vr::drawprobe
