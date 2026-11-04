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
} // namespace m2vr::hostcolor
