#pragma once
// Strict real-original surface mutation failure IDs, never world-pass proof.
#include <cstdint>
namespace m2vr::vrbridge {
// 1=UpdateSurface (COM 30), 2=StretchRect (34), 3=ColorFill (35),
// 4=SetRenderTarget (37), 5=SetDepthStencilSurface (39), 6=SetTransform (44),
// 7=SetVertexShaderConstantF (94), 8=SetViewport (47),
// 9=SetScissorRect (75), 10=SetStreamSource (100), 11=SetIndices (104),
// 12=SetVertexDeclaration (87), 13=SetFVF (89),
// 14=SetVertexShader (92), 15=SetPixelShader (107), 16=SetPixelShaderConstantF (109),
// 17=SetTexture (65), 18=SetTextureStageState (67),
// 19=SetSamplerState (69), 20=SetRenderState (57),
// 21=SetVertexShaderConstantI (96), 22=SetVertexShaderConstantB (98),
// 23=SetPixelShaderConstantI (111), 24=SetPixelShaderConstantB (113),
// 25=SetStreamSourceFreq (102), 26=SetMaterial (49), 27=SetLight (51),
// 28=LightEnable (53), 29=SetClipPlane (55), 30=UpdateTexture (31),
// 31=SetPaletteEntries (71), 32=SetCurrentTexturePalette (73),
// 33=SetClipStatus (62), 34=SetSoftwareVertexProcessing (77),
// 35=SetNPatchMode (79), 36=MultiplyTransform (46),
// 37/38=VB/IB Lock (COM 11), 39/40=VB/IB Unlock (COM 12),
// 41/42=2D LockRect/UnlockRect; 43/44=cube LockRect/UnlockRect;
// 45/46=volume LockBox/UnlockBox (COM slots 19/20);
// 47=GetSurfaceLevel, 48/49=Surface Lock/Unlock, 50/51=GetDC/ReleaseDC;
// 52=cube GetCubeMapSurface, 53=volume GetVolumeLevel,
// 54/55=volume-sublevel LockBox/UnlockBox;
// 56=BaseTexture9 SetAutoGenFilterType, 57=Texture9 AddDirtyRect,
// 58=CubeTexture9 AddDirtyRect, 59=VolumeTexture9 AddDirtyBox;
// 60=CreateTexture, 61=CreateVolumeTexture, 62=CreateCubeTexture;
// 63=CreateVertexBuffer, 64=CreateIndexBuffer:
// original source-state and resource operation failures.
inline constexpr bool IsFailedOriginalD3D9Surface(
    std::uint32_t method,std::uint32_t result_bits)noexcept {
    return method>=1u && method<=64u && (result_bits&0x80000000u)!=0u;
}
inline constexpr std::uint32_t FailedOriginalD3D9SurfaceSourceKind(
    std::uint32_t method)noexcept {
    // Separate from Draw 17..20 and Clear 21.
    return method>=1u && method<=64u?21u+method:0u;
}
} // namespace m2vr::vrbridge
