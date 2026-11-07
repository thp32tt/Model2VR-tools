#include "emulator_window_capture.hpp"
#include "../src/common/win32_desktop_capture.hpp"
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
desktopmenu::WindowRect WindowBounds(HWND window) noexcept {
    RECT r{};
    if(!window || !IsWindow(window) || !GetWindowRect(window,&r))
        return {};
    return {r.left,r.top,r.right,r.bottom};
}
} // namespace

bool EmulatorWindowCapture::Update(std::uint32_t emulator_pid){
    if(!emulator_pid){Invalidate();return false;}
    const auto desktop=desktopmenu::CurrentVirtualDesktop();
    if(!desktop.Valid()){Invalidate();return false;}
    // Keep a PID-authenticated HWND ONLY for optional Quest controller
    // clicking. Desktop capture must remain visible even if the emulator
    // has not created its first window or a ROM dialog is in front.
    const HWND selected=FindEmulatorWindow(emulator_pid);
    const auto captured_foreground=GetForegroundWindow();
    const auto initial_window_rect=WindowBounds(selected);
    const auto now=GetTickCount64();
    if(selected==window_ && desktop.left==source_bounds_.left &&
       desktop.top==source_bounds_.top &&
       desktop.width==source_bounds_.width &&
       desktop.height==source_bounds_.height &&
       now<next_capture_ms_)return ready_;
    window_=selected;
    source_bounds_=desktop;
    ready_=false;
    captured_window_rect_={};
    captured_ms_=0u;
    click_geometry_valid_=false;
    next_capture_ms_=now+100u; // 10fps UI/desktop, never 90Hz CPU readback.
    if(!Capture(desktop))return false;
    // Capture is still useful as a visual-only quad when the original
    // window is absent, occluded or moving. Only controller injection must
    // require stable, currently visible screenshot/window coordinates.
    captured_window_rect_=initial_window_rect;
    captured_ms_=GetTickCount64();
    click_geometry_valid_=selected!=nullptr &&
        captured_foreground==selected && GetForegroundWindow()==selected &&
        desktopmenu::CaptureGeometryMatches(
            desktop,desktopmenu::CurrentVirtualDesktop(),
            initial_window_rect,WindowBounds(selected));
    ready_=true;
    ++serial_;
    if(!serial_)++serial_;
    return true;
}
bool EmulatorWindowCapture::Capture(desktopmenu::DesktopBounds desktop){
    HDC source=GetDC(nullptr);
    if(!source)return false;
    HDC target=CreateCompatibleDC(source);
    if(!target){ReleaseDC(nullptr,source);return false;}
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
        DeleteDC(target);ReleaseDC(nullptr,source);
        return false;
    }
    const HGDIOBJ previous=SelectObject(target,bitmap);
    if(!previous || previous==HGDI_ERROR){
        DeleteObject(bitmap);DeleteDC(target);
        ReleaseDC(nullptr,source);return false;
    }
    // Whole Windows desktop + highlighted real cursor, including when
    // the cursor lies OUTSIDE the small windowed emulator HWND.
    const bool drawn=desktopmenu::CaptureDesktopFrame(
        target,source,desktop,transport::kWidth,transport::kHeight);
    // Quest controller reticle is XR-only and never warps the PC cursor
    // unless the trigger targets a foreground emulator-owned pixel.
    if(drawn && reticle_visible_ &&
       reticle_x_<transport::kWidth && reticle_y_<transport::kHeight){
        HPEN pen=CreatePen(PS_SOLID,2,RGB(0,255,255));
        if(pen){
            HGDIOBJ oldpen=SelectObject(target,pen);
            if(oldpen && oldpen!=HGDI_ERROR){
                const int x=static_cast<int>(reticle_x_);
                const int y=static_cast<int>(reticle_y_);
                MoveToEx(target,x-10,y,nullptr);LineTo(target,x+11,y);
                MoveToEx(target,x,y-10,nullptr);LineTo(target,x,y+11);
                SelectObject(target,oldpen);
            }
            DeleteObject(pen);
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
    SelectObject(target,previous);
    DeleteObject(bitmap);
    DeleteDC(target);
    ReleaseDC(nullptr,source);
    return drawn;
}
bool EmulatorWindowCapture::CapturedClickGeometryLive() const noexcept {
    return click_geometry_valid_ && window_ &&
        desktopmenu::CaptureClickFresh(captured_ms_,GetTickCount64()) &&
        desktopmenu::CaptureGeometryMatches(
            source_bounds_,desktopmenu::CurrentVirtualDesktop(),
            captured_window_rect_,WindowBounds(window_));
}
bool EmulatorWindowCapture::DeliverControllerClick(
    std::uint32_t pid,std::uint32_t desktop_x,std::uint32_t desktop_y){
    if(!ready_ || !window_ || !pid ||
       !CapturedClickGeometryLive() ||
       GetForegroundWindow()!=window_ ||
       !IsWindow(window_) || !IsWindowVisible(window_) || IsIconic(window_))
        return false;
    DWORD owner=0;
    GetWindowThreadProcessId(window_,&owner);
    if(owner!=static_cast<DWORD>(pid))return false;
    RECT rect{};
    if(!GetWindowRect(window_,&rect))return false;
    // The Quest ray is now relative to the ENTIRE virtual desktop,
    // not the top-left of the small emulator window. Preserve the signed
    // multi-monitor origin and refuse clicks outside the emulator HWND.
    const auto mapped=desktopmenu::MapDesktopPoint(
        source_bounds_,desktop_x,desktop_y);
    if(!mapped.valid)return false;
    const POINT target_point{mapped.x,mapped.y};
    if(!PtInRect(&rect,target_point))return false;
    // A topmost overlay or foreign HWND can cover this screen pixel.
    // Never send input to any unrelated app, taskbar or desktop icon.
    DWORD point_pid=0;
    const HWND point_hwnd=WindowFromPoint(target_point);
    if(!point_hwnd || !IsWindow(point_hwnd))return false;
    GetWindowThreadProcessId(point_hwnd,&point_pid);
    if(point_pid!=static_cast<DWORD>(pid))return false;
    // Desktop-wide SetCursorPos preserves negative multi-monitor origins.
    // SendInput is confined to the genuine foreground emulator HWND;
    // never use WM_* messages or inject into the original D3D9 game.
    if(!SetCursorPos(target_point.x,target_point.y))return false;
    // A window can move or the desktop layout can change even while
    // SetCursorPos/WindowFromPoint runs; revalidate before SendInput.
    if(!CapturedClickGeometryLive() || GetForegroundWindow()!=window_)
        return false;
    DWORD post_warp_pid=0;
    const HWND post_warp=WindowFromPoint(target_point);
    if(!post_warp)return false;
    GetWindowThreadProcessId(post_warp,&post_warp_pid);
    if(post_warp_pid!=static_cast<DWORD>(pid))return false;
    INPUT click[2]{};
    click[0].type=INPUT_MOUSE;
    click[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;
    click[1].type=INPUT_MOUSE;
    click[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
    const UINT sent=SendInput(2,click,sizeof(INPUT));
    if(sent==1u){
        // Best-effort release if Windows accepted only the first event.
        SendInput(1,&click[1],sizeof(INPUT));
    }
    if(sent!=2u)return false;
    // Let the next XR menu frame read the resulting dialog/control state
    // immediately rather than retaining the previous 10fps menu image.
    next_capture_ms_=0;
    return true;
}
} // namespace m2vr::host
