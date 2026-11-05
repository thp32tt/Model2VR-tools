#include "emulator_window_capture.hpp"
#include "../src/common/model2_emulator_window_policy.hpp"
#include "../src/common/stereo_frame_ipc.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace m2vr::host {
namespace {
struct WindowSelection {
    DWORD pid=0;
    HWND foreground=nullptr, selected=nullptr, active_dialog=nullptr;
    std::uint64_t best_area=0;
};
BOOL CALLBACK SelectEmulatorWindow(HWND window,LPARAM opaque){
    auto& state=*reinterpret_cast<WindowSelection*>(opaque);
    DWORD pid=0;
    GetWindowThreadProcessId(window,&pid);
    if(pid!=state.pid || !IsWindowVisible(window) || IsIconic(window))
        return TRUE;
    RECT rect{};
    if(!GetWindowRect(window,&rect))return TRUE;
    const auto width=rect.right-rect.left;
    const auto height=rect.bottom-rect.top;
    // Filter hidden control/tool windows, enormous virtual desktops and
    // zero-sized transient popups. Native menu/dialog HWNDs are permitted.
    if(width<160 || height<100 || width>4096 || height>4096)return TRUE;
    const auto area=std::uint64_t(width)*std::uint64_t(height);
    if(window==state.foreground)state.active_dialog=window;
    if(area>state.best_area){state.best_area=area;state.selected=window;}
    return TRUE;
}
HWND FindEmulatorWindow(std::uint32_t pid){
    if(!pid)return nullptr;
    WindowSelection selection{};
    selection.pid=static_cast<DWORD>(pid);
    selection.foreground=GetForegroundWindow();
    EnumWindows(&SelectEmulatorWindow,reinterpret_cast<LPARAM>(&selection));
    // A foreground ROM file chooser must win over the larger emulator
    // window so the player can see the dialog rather than stale game pixels.
    return selection.active_dialog?selection.active_dialog:selection.selected;
}
} // namespace

bool EmulatorWindowCapture::Update(std::uint32_t emulator_pid){
    const HWND selected=FindEmulatorWindow(emulator_pid);
    if(!selected){
        Invalidate();
        return false;
    }
    const auto now=GetTickCount64();
    if(selected==window_ && now<next_capture_ms_)
        return ready_;
    window_=selected;
    ready_=false;
    // Do not perform a cross-process desktop capture at 90/120 Hz; 10 fps
    // is sufficient to read emulator menu and ROM selection dialogs.
    next_capture_ms_=now+100u;
    if(!Capture(selected))return false;
    ready_=true;
    ++serial_;
    if(!serial_)++serial_;
    return true;
}
bool EmulatorWindowCapture::Capture(HWND hwnd){
    RECT rect{};
    if(!GetWindowRect(hwnd,&rect))return false;
    const auto sw=rect.right-rect.left,sh=rect.bottom-rect.top;
    if(sw<160 || sh<100 || sw>4096 || sh>4096)return false;
    const auto dest=desktopmenu::Fit(static_cast<std::uint32_t>(sw),
        static_cast<std::uint32_t>(sh),transport::kWidth,transport::kHeight);
    if(!dest.Valid())return false;
    // Capture the *whole* Win32 HWND, not its client-only D3D surface.
    // File/Open-ROM menu bar and title must appear before a game exists.
    // When it is in the foreground, the desktop DC includes visible native
    // popup menus too; otherwise use the window DC (without forcing focus).
    const HWND foreground=GetForegroundWindow();
    const bool from_desktop=hwnd==foreground;
    const HWND dc_owner=from_desktop?nullptr:hwnd;
    HDC source=from_desktop?GetDC(nullptr):GetWindowDC(hwnd);
    if(!source)return false;
    HDC target=CreateCompatibleDC(source);
    if(!target){ReleaseDC(dc_owner,source);return false;}
    BITMAPINFO bitmap_info{};
    bitmap_info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bitmap_info.bmiHeader.biWidth=static_cast<LONG>(transport::kWidth);
    bitmap_info.bmiHeader.biHeight=-static_cast<LONG>(transport::kHeight);
    bitmap_info.bmiHeader.biPlanes=1;
    bitmap_info.bmiHeader.biBitCount=32;
    bitmap_info.bmiHeader.biCompression=BI_RGB;
    void* bits=nullptr;
    HBITMAP bitmap=CreateDIBSection(target,&bitmap_info,DIB_RGB_COLORS,
                                    &bits,nullptr,0);
    if(!bitmap || !bits){
        if(bitmap)DeleteObject(bitmap);
        DeleteDC(target);ReleaseDC(dc_owner,source);
        return false;
    }
    const auto old=SelectObject(target,bitmap);
    if(!old || old==HGDI_ERROR){
        DeleteObject(bitmap);DeleteDC(target);
        ReleaseDC(dc_owner,source);return false;
    }
    // BI_RGB top-down pixels are 32-bit BGRA on little-endian Windows;
    // prefill opaque black letterboxing and convert to transport RGBA8.
    std::memset(bits,0,transport::kBytesPerEye);
    SetStretchBltMode(target,COLORONCOLOR);
    const BOOL drawn=StretchBlt(target,
        static_cast<int>(dest.x),static_cast<int>(dest.y),
        static_cast<int>(dest.width),static_cast<int>(dest.height),
        source,from_desktop?rect.left:0,from_desktop?rect.top:0,
        sw,sh,SRCCOPY);
    // BitBlt does NOT normally include the software cursor. Blend the
    // user's genuine Win32 cursor onto the virtual menu while this exact
    // emulator HWND owns foreground focus; do NOT impersonate a Quest
    // controller pointer, inject clicks or change emulator window focus.
    if(drawn && from_desktop){
        CURSORINFO cursor{};
        cursor.cbSize=sizeof(cursor);
        if(GetCursorInfo(&cursor) &&
           (cursor.flags&CURSOR_SHOWING)!=0 && cursor.hCursor){
            ICONINFO icon{};
            if(GetIconInfo(cursor.hCursor,&icon)){
                const auto pointer=desktopmenu::PlacePointer(
                    cursor.ptScreenPos.x,cursor.ptScreenPos.y,
                    rect.left,rect.top,
                    static_cast<std::uint32_t>(sw),
                    static_cast<std::uint32_t>(sh),dest,
                    static_cast<std::uint32_t>(GetSystemMetrics(SM_CXCURSOR)),
                    static_cast<std::uint32_t>(GetSystemMetrics(SM_CYCURSOR)),
                    icon.xHotspot,icon.yHotspot);
                if(pointer.Valid())
                    DrawIconEx(target,pointer.x,pointer.y,cursor.hCursor,
                        static_cast<int>(pointer.width),
                        static_cast<int>(pointer.height),
                        0,nullptr,DI_NORMAL);
                if(icon.hbmMask)DeleteObject(icon.hbmMask);
                if(icon.hbmColor)DeleteObject(icon.hbmColor);
            }
        }
    }
    if(drawn){
        rgba_.resize(transport::kBytesPerEye);
        const auto* bgra=static_cast<const std::uint8_t*>(bits);
        for(std::size_t i=0;i<rgba_.size();i+=4u){
            rgba_[i]=bgra[i+2u];
            rgba_[i+1u]=bgra[i+1u];
            rgba_[i+2u]=bgra[i];
            rgba_[i+3u]=255u;
        }
    }
    SelectObject(target,old);
    DeleteObject(bitmap);
    DeleteDC(target);
    ReleaseDC(dc_owner,source);
    return drawn!=0;
}
} // namespace m2vr::host
