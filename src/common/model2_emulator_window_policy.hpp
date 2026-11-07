#pragma once
// Desktop Model 2 menu is a visual 2D source, never verified game stereo.
// Independent of Windows, OpenXR and game/ROM content for regression tests.
#include <cstdint>
#include <limits>
namespace m2vr::desktopmenu {
// A virtual Windows desktop can span monitors LEFT/ABOVE the primary.
 // All screenshot and Quest ray/click coordinates must share this origin.
struct DesktopBounds {
    std::int32_t left=0,top=0;
    std::uint32_t width=0,height=0;
    constexpr bool Valid()const noexcept {
        return width>=160u && height>=100u &&
               width<=8192u && height<=8192u;
    }
};
// One Quest menu click must target the SAME desktop geometry and emulator
// HWND rectangle shown in its most recent 10-fps desktop-capture image.
// A monitor hotplug, resolution/DPI move, or ROM-dialog relocation can make
// the old XR quad coordinates point at the wrong Windows control even if the
// HWND still belongs to the authenticated original emulator process.
struct WindowRect {
    std::int32_t left=0,top=0,right=0,bottom=0;
    constexpr bool Valid()const noexcept {
        return right>left && bottom>top;
    }
};
constexpr bool CaptureGeometryMatches(
    DesktopBounds captured,DesktopBounds live,
    WindowRect pictured,WindowRect current) noexcept {
    return captured.Valid() && live.Valid() &&
           captured.left==live.left && captured.top==live.top &&
           captured.width==live.width && captured.height==live.height &&
           pictured.Valid() && current.Valid() &&
           pictured.left==current.left && pictured.top==current.top &&
           pictured.right==current.right && pictured.bottom==current.bottom;
}
// The 10-fps source may be skipped for one XR tick, but never inject an OS
// click against a stale 2D snapshot even when the window itself remains.
constexpr bool CaptureClickFresh(std::uint64_t captured_ms,
                                 std::uint64_t now_ms) noexcept {
    return captured_ms!=0u && now_ms>=captured_ms &&
           now_ms-captured_ms<=250u;
}
// Never inject a Quest menu click against a desktop capture that has not
// reached a PREVIOUS successful xrEndFrame. A newer 10-fps screenshot can
// change the visible ROM/menu controls before the HMD shows those pixels.
// Non-menu frames and XR session invalidation reset the submitted serial.
constexpr bool PresentedCaptureMatches(std::uint64_t current_capture_serial,
                                       std::uint64_t submitted_menu_serial) noexcept {
    return current_capture_serial!=0u &&
           current_capture_serial==submitted_menu_serial;
}
// Windows may report a foreground emulator PID for two different top-level
// HWNDs. Child controls of the photographed foreground HWND are click-safe,
// but an overlapping sibling popup/window is not, even under the same PID.
// Compare Win32 GetAncestor(hit, GA_ROOT) with the captured top-level HWND.
constexpr bool CapturedRootMatches(std::uintptr_t captured,
                                   std::uintptr_t hit_root) noexcept {
    return captured!=0u && hit_root!=0u && captured==hit_root;
}
struct ScreenPoint {
    std::int32_t x=0,y=0;
    bool valid=false;
};
// SetCursorPos() may report success but Windows can CLIP the cursor to a
// different position (ClipCursor), including another control of the SAME
// emulator process. The actual OS hotspot must match the Quest ray's exact
// signed virtual-desktop destination before any injected mouse-down/up.
constexpr bool CursorPositionMatches(ScreenPoint requested,
                                     std::int32_t actual_x,
                                     std::int32_t actual_y) noexcept {
    return requested.valid &&
           requested.x==actual_x && requested.y==actual_y;
}
constexpr ScreenPoint MapDesktopPoint(DesktopBounds desktop,
                                      std::uint32_t source_x,
                                      std::uint32_t source_y) noexcept {
    if(!desktop.Valid() || source_x>=desktop.width ||
       source_y>=desktop.height)return {};
    const auto x=std::int64_t(desktop.left)+source_x;
    const auto y=std::int64_t(desktop.top)+source_y;
    if(x>std::numeric_limits<std::int32_t>::max() ||
       y>std::numeric_limits<std::int32_t>::max() ||
       x<std::numeric_limits<std::int32_t>::min() ||
       y<std::numeric_limits<std::int32_t>::min())return {};
    return {static_cast<std::int32_t>(x),static_cast<std::int32_t>(y),true};
}
struct Letterbox {
    std::uint32_t x=0, y=0, width=0, height=0;
    constexpr bool Valid()const noexcept{return width!=0u && height!=0u;}
};
// Fill the fixed square XR quad with a correctly shaped *complete* emulator
// window including its native title bar, menu and any visible game chooser.
// Integer-safe nearest sizing; never crop a source window to fill the square.
constexpr Letterbox Fit(std::uint32_t source_width,std::uint32_t source_height,
                        std::uint32_t target_width,std::uint32_t target_height) noexcept {
    if(!source_width || !source_height || !target_width || !target_height ||
       source_width>8192u || source_height>8192u ||
       target_width>8192u || target_height>8192u)return {};
    const auto wide=std::uint64_t(source_width)*target_height>=
                    std::uint64_t(source_height)*target_width;
    const std::uint32_t width=wide?target_width:
        static_cast<std::uint32_t>(
            (std::uint64_t(target_height)*source_width)/source_height);
    const std::uint32_t height=wide?
        static_cast<std::uint32_t>(
            (std::uint64_t(target_width)*source_height)/source_width):
        target_height;
    if(!width || !height)return {};
    return {(target_width-width)/2u,(target_height-height)/2u,width,height};
}
// The OS mouse pointer is not embedded in desktop GDI BitBlt pixels.
// Paint its *current* window-local hotspot inside the aspect-preserved
// emulator texture. Coordinates may be negative on secondary monitors.
struct PointerRect {
    std::int32_t x=0,y=0;
    std::uint32_t width=0,height=0;
    constexpr bool Valid()const noexcept{return width!=0u && height!=0u;}
};
constexpr PointerRect PlacePointer(std::int32_t screen_x,std::int32_t screen_y,
    std::int32_t window_left,std::int32_t window_top,
    std::uint32_t window_width,std::uint32_t window_height,
    Letterbox target,std::uint32_t pointer_width,std::uint32_t pointer_height,
    std::uint32_t hotspot_x,std::uint32_t hotspot_y) noexcept {
    if(!target.Valid() || window_width==0u || window_height==0u ||
       window_width>8192u || window_height>8192u ||
       pointer_width==0u || pointer_height==0u ||
       pointer_width>256u || pointer_height>256u ||
       hotspot_x>=pointer_width || hotspot_y>=pointer_height)return {};
    const auto local_x=std::int64_t(screen_x)-window_left;
    const auto local_y=std::int64_t(screen_y)-window_top;
    if(local_x<0 || local_y<0 ||
       local_x>=window_width || local_y>=window_height)return {};
    const auto scaled_w=static_cast<std::uint32_t>(
        (std::uint64_t(pointer_width)*target.width)/window_width);
    const auto scaled_h=static_cast<std::uint32_t>(
        (std::uint64_t(pointer_height)*target.height)/window_height);
    const auto width=scaled_w?scaled_w:1u,height=scaled_h?scaled_h:1u;
    const auto x=std::int64_t(target.x)+
        (local_x*target.width)/window_width-
        (std::uint64_t(hotspot_x)*target.width)/window_width;
    const auto y=std::int64_t(target.y)+
        (local_y*target.height)/window_height-
        (std::uint64_t(hotspot_y)*target.height)/window_height;
    return {static_cast<std::int32_t>(x),static_cast<std::int32_t>(y),
            width,height};
}
enum class Source : std::uint8_t { Diagnostic, EmulatorWindow2D, GameMono2D, VerifiedGameStereo };
constexpr Source Choose(bool fresh_game_pair,bool game_pair_is_stereo,
                        bool fresh_emulator_window) noexcept {
    if(fresh_game_pair)
        return game_pair_is_stereo?Source::VerifiedGameStereo:Source::GameMono2D;
    return fresh_emulator_window?Source::EmulatorWindow2D:Source::Diagnostic;
}
constexpr std::uint64_t MenuUploadToken(std::uint64_t capture_serial) noexcept {
    return capture_serial?((std::uint64_t{1}<<63)|
                           (capture_serial&((std::uint64_t{1}<<63)-1u))):0u;
}
} // namespace m2vr::desktopmenu
