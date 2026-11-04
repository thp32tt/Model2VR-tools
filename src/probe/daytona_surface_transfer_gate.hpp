#pragma once
// An original D3D9 backbuffer surface call has already succeeded. Allow
// a per-eye copy only if the original source is still trustworthy and an
// initial partial write cannot expose uninitialized eye surface pixels.
#include <Windows.h>
#include <cstdint>
namespace m2vr::daytona {
inline bool FullSurfaceRect(const RECT* rect,std::uint32_t width,
                            std::uint32_t height)noexcept{
    if(!width || !height)return false;
    return !rect || (rect->left==0 && rect->top==0 &&
           rect->right==static_cast<LONG>(width) &&
           rect->bottom==static_cast<LONG>(height));
}
inline bool CanMirrorSurfaceTransfer(bool dest_is_backbuffer,
                                     bool source_is_external,
                                     bool full_destination,
                                     bool eye_frame_started)noexcept{
    return dest_is_backbuffer && source_is_external &&
           (eye_frame_started || full_destination);
}
// Prove that the source subrectangle maps into the destination without
// invoking undefined signed arithmetic or exposing out-of-bounds eye writes.
inline bool UpdateSurfaceBounds(const RECT* source_rect,
                                std::uint32_t source_width,
                                std::uint32_t source_height,
                                const POINT* destination_point,
                                std::uint32_t destination_width,
                                std::uint32_t destination_height,
                                bool& full_destination)noexcept{
    full_destination=false;
    if(!source_width||!source_height||!destination_width||!destination_height||
       source_width>0x7fffffffu||source_height>0x7fffffffu||
       destination_width>0x7fffffffu||destination_height>0x7fffffffu)return false;
    const RECT r=source_rect?*source_rect:RECT{0,0,
        static_cast<LONG>(source_width),static_cast<LONG>(source_height)};
    if(r.left<0||r.top<0||r.right<=r.left||r.bottom<=r.top||
       static_cast<std::uint32_t>(r.right)>source_width||
       static_cast<std::uint32_t>(r.bottom)>source_height)return false;
    const LONG x=destination_point?destination_point->x:0;
    const LONG y=destination_point?destination_point->y:0;
    if(x<0||y<0)return false;
    const auto w=static_cast<std::uint32_t>(r.right-r.left);
    const auto h=static_cast<std::uint32_t>(r.bottom-r.top);
    if(std::uint64_t{static_cast<std::uint32_t>(x)}+w>destination_width||
       std::uint64_t{static_cast<std::uint32_t>(y)}+h>destination_height)return false;
    full_destination=x==0&&y==0&&w==destination_width&&h==destination_height;
    return true;
}
} // namespace m2vr::daytona
