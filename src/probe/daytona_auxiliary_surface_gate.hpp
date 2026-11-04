#pragma once
// A single original full-size auxiliary D3D9 RT may be mirrored only after
// bounded format/depth verification and a complete source color clear.
// Never reinterpret small menu/mirror/postprocess surfaces as game world.
#include <d3d9.h>
#include <cstdint>
namespace m2vr::daytona {
inline bool SourceCompatibleWithMain(const D3DSURFACE_DESC& source,
                                     const D3DSURFACE_DESC& depth,
                                     const D3DSURFACE_DESC& main)noexcept {
    return source.Pool==D3DPOOL_DEFAULT &&
           (source.Usage&D3DUSAGE_RENDERTARGET)!=0u &&
           source.Width==main.Width && source.Height==main.Height &&
           source.Format==main.Format &&
           source.MultiSampleType==main.MultiSampleType &&
           source.MultiSampleQuality==main.MultiSampleQuality &&
           depth.Pool==D3DPOOL_DEFAULT &&
           (depth.Usage&D3DUSAGE_DEPTHSTENCIL)!=0u &&
           depth.Width==source.Width && depth.Height==source.Height &&
           depth.MultiSampleType==source.MultiSampleType &&
           depth.MultiSampleQuality==source.MultiSampleQuality;
}
// An original full-size auxiliary render target can be the first source.
// This gate prevents small HUD/mirror targets from initializing eye clones.
inline bool BootstrapTargetCompatible(const D3DSURFACE_DESC& active,
                                      const D3DSURFACE_DESC& depth,
                                      const D3DSURFACE_DESC& back)noexcept{
    return SourceCompatibleWithMain(active,depth,back);
}
inline bool SameEyeDepthLayout(const D3DSURFACE_DESC& a,
                               const D3DSURFACE_DESC& b)noexcept{
    return a.Pool==D3DPOOL_DEFAULT && b.Pool==D3DPOOL_DEFAULT &&
           (a.Usage&D3DUSAGE_DEPTHSTENCIL)!=0u &&
           (b.Usage&D3DUSAGE_DEPTHSTENCIL)!=0u &&
           a.Width==b.Width && a.Height==b.Height &&
           a.Format==b.Format &&
           a.MultiSampleType==b.MultiSampleType &&
           a.MultiSampleQuality==b.MultiSampleQuality;
}
inline bool FullOriginalViewport(const D3DVIEWPORT9& viewport,
                                 UINT width,UINT height)noexcept {
    return width>0 && height>0 && viewport.X==0 && viewport.Y==0 &&
           viewport.Width==width && viewport.Height==height;
}
inline bool FullInitialColorClear(DWORD count,const D3DRECT* rects,
                                  DWORD flags,UINT width,UINT height)noexcept {
    if(!(flags&D3DCLEAR_TARGET)||!width||!height)return false;
    // The exact original source Clear is already successful. A partial
    // Clear cannot initialize the rest of a new GPU clone without leaks.
    if(count==0)return true;
    return count==1 && rects && rects[0].x1==0u && rects[0].y1==0u &&
           rects[0].x2==width && rects[0].y2==height;
}
enum class MainBlitSource:std::uint8_t {
    Skip, FlatSource, StereoAuxiliary
};
inline MainBlitSource RouteOriginalToMain(
     bool dest_is_main,bool src_is_main,bool src_is_aux,
     bool aux_frame_ready,bool full_destination,bool main_started)noexcept{
    if(!dest_is_main || src_is_main ||
       (!full_destination && !main_started))return MainBlitSource::Skip;
    // If an auxiliary is known, never silently flatten an incomplete
    // auxiliary frame over the independently rendered eye world.
    if(src_is_aux)
        return aux_frame_ready ? MainBlitSource::StereoAuxiliary:
                                 MainBlitSource::Skip;
    return MainBlitSource::FlatSource;
}
} // namespace m2vr::daytona
