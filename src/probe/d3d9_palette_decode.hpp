#pragma once
// CPU-lockable original P8/A8P8 D3D9 stage-zero textures. Palette selection
// and alpha capability are read from the real original D3D9 device without
// SetPalette, a GPU draw, or any change to the emulator's source resources.
#include <array>
#include <cstdint>
#include <d3d9.h>

namespace m2vr::drawprobe {
struct CpuPaletteRGBA {
    std::array<std::array<std::uint8_t,4>,256> colors{};
    bool palette_alpha_supported=false;
};
inline constexpr std::uint32_t PalettedPixelBytes(D3DFORMAT format) noexcept {
    return format==D3DFMT_P8?1u:format==D3DFMT_A8P8?2u:0u;
}
inline bool DecodePalettedRowRGBA(const std::uint8_t* source,
                                  std::uint8_t* output,
                                  std::uint32_t width,D3DFORMAT format,
                                  const CpuPaletteRGBA& palette) noexcept {
    const auto source_bytes=PalettedPixelBytes(format);
    if(!source || !output || !source_bytes || !width || width>1024u)
        return false;
    for(std::uint32_t x=0u;x<width;++x){
        const std::uint32_t index=source[std::size_t(x)*source_bytes];
        const auto& rgba=palette.colors[index];
        const std::size_t dst=std::size_t(x)*4u;
        output[dst]=rgba[0];
        output[dst+1u]=rgba[1];
        output[dst+2u]=rgba[2];
        // A8P8 alpha is from the pixel's high byte. Only P8 can use
        // palette peFlags as alpha and only when ALPHAPALETTE is supported.
        output[dst+3u]=source_bytes==2u ?
            source[std::size_t(x)*2u+1u] :
            (palette.palette_alpha_supported?rgba[3]:255u);
    }
    return true;
}
} // namespace m2vr::drawprobe
