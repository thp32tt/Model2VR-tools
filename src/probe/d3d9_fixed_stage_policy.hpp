#pragma once
// Exact fixed-function D3D9 stage-zero TEXTURE/DIFFUSE operation contract.
// Unknown operations (TFACTOR, COMPLEMENT, ADD, DOT3, disabled source, etc.)
// are not approximated inside a calibrated native stereo world pass.
#include <d3d9.h>

namespace m2vr::drawprobe {
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
