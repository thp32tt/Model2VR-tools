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
} // namespace m2vr::daytona
