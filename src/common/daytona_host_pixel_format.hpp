#pragma once
// Strict host-side negotiation between the source RGBA8 GameStereo/CompactMono
// wire ABI and the actual DXGI texture format offered by the OpenXR runtime.
// Never mutate original Win32 game buffers or reinterpret BGRA as RGBA.
#include <cstddef>
#include <cstdint>
namespace m2vr::hostcolor {
inline std::int64_t ChooseSwapchainFormat(const std::int64_t* supported,
                                          std::size_t count,
                                          std::int64_t rgba,
                                          std::int64_t bgra) noexcept {
    if(!supported || !count || rgba<=0 || bgra<=0 || rgba==bgra)return 0;
    for(std::size_t i=0;i<count;++i)
        if(supported[i]==rgba)return rgba;
    for(std::size_t i=0;i<count;++i)
        if(supported[i]==bgra)return bgra;
    return 0;
}
// Source is ALWAYS RGBA8; BGRA8 is only a runtime-selected upload layout.
// Do not swizzle in place: this preserves the accepted atomic IPC pair.
inline bool ConvertRgbaToBgra(const std::uint8_t* source,std::size_t bytes,
                              std::uint8_t* destination,
                              std::size_t capacity) noexcept {
    if(!source || !destination || source==destination || bytes==0 ||
       (bytes&3u)!=0u || capacity<bytes)return false;
    for(std::size_t i=0;i<bytes;i+=4u){
        destination[i+0u]=source[i+2u];
        destination[i+1u]=source[i+1u];
        destination[i+2u]=source[i+0u];
        destination[i+3u]=source[i+3u];
    }
    return true;
}

 // Convert only the requested box inside a full-stride RGBA/BGRA image.
 // The src/dst buffers remain separate; untouched destination bytes persist.
inline bool ConvertRgbaToBgraRect(const std::uint8_t* source,
    std::size_t source_capacity,std::uint8_t* destination,
    std::size_t destination_capacity,std::uint32_t width,
    std::uint32_t height,std::uint32_t left,std::uint32_t top,
    std::uint32_t right,std::uint32_t bottom) noexcept {
    if(!source || !destination || source==destination ||
       !width || !height || width>8192u || height>8192u ||
       left>=right || top>=bottom || right>width || bottom>height ||
       std::uint64_t(width)*height*4u>source_capacity ||
       std::uint64_t(width)*height*4u>destination_capacity)return false;
    for(std::uint32_t y=top;y<bottom;++y)
        for(std::uint32_t x=left;x<right;++x){
            const auto at=(std::size_t(y)*width+x)*4u;
            destination[at+0u]=source[at+2u];
            destination[at+1u]=source[at+1u];
            destination[at+2u]=source[at+0u];
            destination[at+3u]=source[at+3u];
        }
    return true;
}
} // namespace m2vr::hostcolor
