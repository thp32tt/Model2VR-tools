#pragma once
// Startup/ROM UI: capture the ENTIRE visible Windows virtual desktop,
// including other HWNDs, menus, taskbar and the genuine mouse cursor.
// This is a local opt-in Quest visual surface, NOT a gameplay World3D pass.
// No input, focus, FFB, emulator EXE or window sizes are changed here.
#include "model2_emulator_window_policy.hpp"
#include <Windows.h>
#include <algorithm>
#include <cstdint>

namespace m2vr::desktopmenu {
inline DesktopBounds CurrentVirtualDesktop() noexcept {
    const int x=GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y=GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int w=GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int h=GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if(w<160 || h<100 || w>8192 || h>8192)return {};
    return {x,y,static_cast<std::uint32_t>(w),
                static_cast<std::uint32_t>(h)};
}
inline void DrawVisibleDesktopCursor(HDC target,DesktopBounds desktop,
                                    Letterbox dest) noexcept {
    CURSORINFO cursor{};
    cursor.cbSize=sizeof(cursor);
    if(!GetCursorInfo(&cursor) || !(cursor.flags&CURSOR_SHOWING) ||
       !cursor.hCursor)return;
    ICONINFO icon{};
    if(!GetIconInfo(cursor.hCursor,&icon))return;
    const auto logical_width=static_cast<std::uint32_t>((std::max)(
        GetSystemMetrics(SM_CXCURSOR),static_cast<int>(icon.xHotspot+1u)));
    const auto logical_height=static_cast<std::uint32_t>((std::max)(
        GetSystemMetrics(SM_CYCURSOR),static_cast<int>(icon.yHotspot+1u)));
    const auto pointer=PlacePointer(cursor.ptScreenPos.x,
        cursor.ptScreenPos.y,desktop.left,desktop.top,desktop.width,
        desktop.height,dest,logical_width,logical_height,
        icon.xHotspot,icon.yHotspot);
    if(pointer.Valid()){
        // 3440px desktop -> 1024px XR image shrinks a normal arrow to ~10px.
        // Keep the *hotspot* at its exact mapped screen coordinate while
        // enlarging only the visible cursor artwork (minimum 44 XR pixels).
        const int width=static_cast<int>((std::clamp)(
            pointer.width,44u,96u));
        const int height=static_cast<int>((std::clamp)(
            pointer.height,44u,96u));
        const int tip_x=pointer.x+static_cast<int>(
            (std::uint64_t(icon.xHotspot)*pointer.width)/logical_width);
        const int tip_y=pointer.y+static_cast<int>(
            (std::uint64_t(icon.yHotspot)*pointer.height)/logical_height);
        const int draw_x=tip_x-static_cast<int>(
            (std::uint64_t(icon.xHotspot)*width)/logical_width);
        const int draw_y=tip_y-static_cast<int>(
            (std::uint64_t(icon.yHotspot)*height)/logical_height);
        // Small high-contrast ring makes the pointer visible on light
        // desktop backgrounds. It does not warp or move the real OS cursor.
        HPEN ring=CreatePen(PS_SOLID,2,RGB(255,196,0));
        if(ring){
            HGDIOBJ prior_pen=SelectObject(target,ring);
            HGDIOBJ prior_brush=SelectObject(target,GetStockObject(HOLLOW_BRUSH));
            if(prior_pen && prior_pen!=HGDI_ERROR &&
               prior_brush && prior_brush!=HGDI_ERROR){
                Ellipse(target,tip_x-13,tip_y-13,tip_x+14,tip_y+14);
            }
            if(prior_brush && prior_brush!=HGDI_ERROR)
                SelectObject(target,prior_brush);
            if(prior_pen && prior_pen!=HGDI_ERROR)
                SelectObject(target,prior_pen);
            DeleteObject(ring);
        }
        DrawIconEx(target,draw_x,draw_y,cursor.hCursor,width,height,
                   0,nullptr,DI_NORMAL);
    }
    if(icon.hbmMask)DeleteObject(icon.hbmMask);
    if(icon.hbmColor)DeleteObject(icon.hbmColor);
}
inline bool CaptureDesktopFrame(HDC target,HDC screen,
                                DesktopBounds desktop,
                                std::uint32_t target_width,
                                std::uint32_t target_height) noexcept {
    if(!target || !screen || !desktop.Valid())return false;
    const auto dest=Fit(desktop.width,desktop.height,target_width,target_height);
    if(!dest.Valid())return false;
    const RECT full{0,0,static_cast<LONG>(target_width),
                    static_cast<LONG>(target_height)};
    FillRect(target,&full,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    SetStretchBltMode(target,COLORONCOLOR);
    const auto draw=[&](DWORD raster_op){
        return StretchBlt(target,static_cast<int>(dest.x),
            static_cast<int>(dest.y),static_cast<int>(dest.width),
            static_cast<int>(dest.height),screen,desktop.left,desktop.top,
            static_cast<int>(desktop.width),
            static_cast<int>(desktop.height),raster_op)!=FALSE;
    };
    // CAPTUREBLT includes layered popups when supported. Falling back to
    // plain SRCCOPY is safer than losing the user's whole XR startup screen.
    const bool copied=draw(SRCCOPY|CAPTUREBLT) || draw(SRCCOPY);
    if(copied)DrawVisibleDesktopCursor(target,desktop,dest);
    return copied;
}
} // namespace m2vr::desktopmenu
