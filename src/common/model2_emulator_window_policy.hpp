#pragma once
// Desktop Model 2 menu is a visual 2D source, never verified game stereo.
// Independent of Windows, OpenXR and game/ROM content for regression tests.
#include <cstdint>
namespace m2vr::desktopmenu {
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
