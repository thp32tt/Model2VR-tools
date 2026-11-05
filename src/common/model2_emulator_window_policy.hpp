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
